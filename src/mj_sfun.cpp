// Copyright 2022-2023 The MathWorks, Inc.
#define S_FUNCTION_NAME  mj_sfun
#define S_FUNCTION_LEVEL 2
#include "simstruc.h"

#include "mj.hpp"
#include <string>
#include <stdio.h>
#include <thread>
#include <vector>
#include <mutex>
#include <memory>

// CONSTANT LIMITS
#define FILE_PATH_LIMIT 1000
#define PORT_STRING_LMT 1000
#define ERROR_LMT 100
#define PARAM_STRING_LIMIT 100

/* S-Function parameter indices */
typedef enum {
    XML_PARAM_INDEX = 0,
    RENDERING_INDEX,
    CONTROL_LENGTH_INDEX, 
    SENSOR_LENGTH_INDEX, 
    RGB_LENGTH_INDEX, 
    DEPTH_LENGTH_INDEX,
    VSYNC_INDEX, 
    VISUAL_FPS_INDEX, 
    CAMERA_SAMPLETIME_INDEX,
    BLOCK_SAMPLETIME_INDEX,
    ZOOM_LEVEL_INDEX,
    PARAM_COUNT
} paramIdx;

// Input indices
typedef enum {
    CONTROL_PORT_INDEX = 0,
    INPORT_COUNT
} inportIndex;

// output indices
typedef enum {
    SENSOR_PORT_INDEX = 0,
    RGB_PORT_INDEX,
    DEPTH_PORT_INDEX,
    OUTPORT_COUNT
} outportIndex;

typedef enum 
{
    MI_IW_IDX=0,
    MG_IW_IDX,
    IWORK_COUNT
}iWorkIndex;

typedef enum
{
    RENDERING_LOCAL=0,
    RENDERING_GLOBAL,
    RENDERING_NONE
}renderingTypeEnum;

using std::vector;
using std::mutex;
using std::shared_ptr;
using std::make_shared;
void renderingThreadFcn();

// Mutexes inside mujocoModelInstance cannot be copied or moved. So address of mi is stored and moved inside vector
class _StaticData
{
    public:
    vector<shared_ptr<MujocoModelInstance>> mi;
    mutex miInitMutex; // used only during initialization

    vector<shared_ptr<MujocoGUI>> mg;
    mutex mgInitMutex;

    guiErrCodes renderingInitErr = NO_ERR;
    mutex renderingInitErrMutex;

    std::thread renderingThread;
    std::atomic<bool> renderingThreadStarted = false;
    std::atomic<bool> signalThreadExit = false;

    // Window management
    bool leftButton = false;
    bool rightButton = false;
    double lastMouseX = 0;
    double lastMouseY = 0;

    public:
    void deleter()
    {
        // clear the blocks memory after each simulation
        mg.clear();
        mi.clear();
        renderingInitErr = NO_ERR;
        renderingThreadStarted = false;
        signalThreadExit = false;

        leftButton = false;
        rightButton = false;
        lastMouseX = 0;
        lastMouseY = 0;
    }
} sd;
std::mutex sdMutex; // use it only when deleting the whole static data

std::atomic<unsigned long> activeSimulinkBlocksCount{0};

/* mi and mg data within are protected already by mutexes
    But the vector as a whole is not protected.
    Make sure mi and mg vector are protected during the phase they can change (init)
*/

// END OF STATIC/GLOBAL Variables---------------------------


// WINDOW CALLBACK MANAGEMENT ---------------------------
// Based on MuJoCo's sample code basic.cc

// Mouse callbacks
int getActiveWindowIndex(GLFWwindow* window)
{
    int activeGuiIndex = 0;
    for(int i=0; i<sd.mg.size(); i++)
    {
        if(sd.mg[i]->window == window)
        {
            activeGuiIndex = i;
            break;
        }
    }
    return activeGuiIndex;
}

static void mouseMoveCallback(GLFWwindow* window, double x, double y)
{
    int activeGuiIndex = getActiveWindowIndex(window);
    // If mouse just moves, do not do anything
    if(!sd.leftButton && !sd.rightButton)
    {
        return;
    }

    double dx = x - sd.lastMouseX;
    double dy = y - sd.lastMouseY;
    sd.lastMouseX = x;
    sd.lastMouseY = y;

    bool shiftKeyStatus = (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT)==GLFW_PRESS || glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT)==GLFW_PRESS);

    int width, height;
    glfwGetWindowSize(window, &width, &height);

    mjtMouse action;
    if(sd.rightButton)
    {
        action = shiftKeyStatus ? mjMOUSE_MOVE_H : mjMOUSE_MOVE_V;
    }
    else if(sd.leftButton)
    {
        action = shiftKeyStatus ? mjMOUSE_ROTATE_H : mjMOUSE_ROTATE_V;
    }
    else
    {
        action = mjMOUSE_ZOOM;
    }

    auto &gui = sd.mg[activeGuiIndex];
    mjv_moveCamera(gui->sceneAssetModel->get_m(), action, dx/height, dy/height, &gui->scn, &gui->cam);
}

static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
    sd.leftButton = (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT)==GLFW_PRESS);
    sd.rightButton = (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT)==GLFW_PRESS);
    glfwGetCursorPos(window, &sd.lastMouseX, &sd.lastMouseY);

    // Nothing else to be done for mouse clicks
}

static void scrollCallback(GLFWwindow *window, double xoffset, double yoffset)
{
    int activeGuiIndex = getActiveWindowIndex(window);
    auto &gui = sd.mg[activeGuiIndex];
    mjv_moveCamera(gui->sceneAssetModel->get_m(), mjMOUSE_ZOOM, 0, -0.05*yoffset, &gui->scn, &gui->cam);
}

// void glfwCallbackInit()
// {
//     glfwSetCursorPosCallback(window, NULL);
//     glfwSetMouseButtonCallback(window, NULL);
//     glfwSetScrollCallback(window, NULL);
// }

// void window_focus_callback(GLFWwindow* window, int focused)
// {
//     if (focused)
//     {
//         getActiveWindowIndex(window);
//         // The window gained input focus
//         glfwSetCursorPosCallback(window, mouseMoveCallback);
//         glfwSetMouseButtonCallback(window, mouseButtonCallback);
//         glfwSetScrollCallback(window, scrollCallback);
//     }
//     else
//     {
//         // The window lost input focus
//         sd.activeGuiIndex = 0;

//     }
// }

// HELPERS -----------------------------------------------------
std::string getXmlFilePath(SimStruct *S)
{
    const mxArray *fileMexPtr = ssGetSFcnParam(S, XML_PARAM_INDEX);
    char file[FILE_PATH_LIMIT];
    mxGetString(fileMexPtr, file, FILE_PATH_LIMIT-1);
    return std::string(file);
}

int getIntParam(SimStruct *S, int index)
{
    const mxArray *mexPtr = ssGetSFcnParam(S, index);
    double param = mxGetScalar(mexPtr);
    return static_cast<int>(param);
}

// MODEL INIT ---------------------------------------------------------
static void mdlInitializeSizes(SimStruct *S)
{
    // ssPrintf("mdlInitializeSizes entered\n");

    //BASIC PARAMETERS --------------------------------------------------------------------------------
    // parameter sizes
    ssSetNumSFcnParams(S, (int)PARAM_COUNT);

    if (ssGetNumSFcnParams(S) != ssGetSFcnParamsCount(S)) {
        if (ssGetErrorStatus(S) != NULL) {
            return;
        }
        return; /* Parameter mismatch will be reported by Simulink */
    }
    
    // sample times
    ssSetNumSampleTimes(S, 1);
    
    /* specify the sim state compliance to be same as a built-in block */
    ssSetOperatingPointCompliance(S, OPERATING_POINT_COMPLIANCE_UNKNOWN);

    /* Set this S-function as runtime thread-safe for multicore execution */
    ssSetRuntimeThreadSafetyCompliance(S, RUNTIME_THREAD_SAFETY_COMPLIANCE_TRUE );

    // Set all parameter as non-tunable
    for(int index = 0; index<PARAM_COUNT; index++)
    {
        ssSetSFcnParamTunable(S, index,  SS_PRM_NOT_TUNABLE);
    }
    
   
    ssSetOptions(S,
                 SS_OPTION_EXCEPTION_FREE_CODE|
                 SS_OPTION_DISCRETE_VALUED_OUTPUT);
    // TODO - not fully exception free. Audit the code for potential exceptions

    ssSupportsMultipleExecInstances(S, true); // support for-each subsystem

    // Operating point compliance is required for enabling fast restart. 
    // But no idea what it means or how to check for it.
    // TODO - support fast restart
    // ssSetOperatingPointCompliance(S, DISALLOW_OPERATING_POINT);

    if (!ssSetNumInputPorts(S, INPORT_COUNT)) return;
    ssSetInputPortWidth(S, CONTROL_PORT_INDEX, getIntParam(S, CONTROL_LENGTH_INDEX) + 1);
    // Last element is a dummy. In case we have a empty count, we will still show a dummy port in S function and handle the nuances in ML/SL layer

    ssSetInputPortDirectFeedThrough(S, CONTROL_PORT_INDEX, 0); // input does not affect the output in the same time step
    ssSetInputPortComplexSignal(S, CONTROL_PORT_INDEX, COMPLEX_NO);
    ssSetInputPortDataType(S, CONTROL_PORT_INDEX, SS_DOUBLE);

    // sensor output
    if (!ssSetNumOutputPorts(S, OUTPORT_COUNT)) return;

    ssSetOutputPortWidth(S, SENSOR_PORT_INDEX, getIntParam(S, SENSOR_LENGTH_INDEX) + 1);
    // last index is a dummy. In case sensor count is 0, it will still let us keep sensor as dummy port.
    ssSetOutputPortDataType(S, SENSOR_PORT_INDEX, SS_DOUBLE);

    // camera output
    ssSetOutputPortWidth(S, RGB_PORT_INDEX, getIntParam(S, RGB_LENGTH_INDEX) + 1);
    ssSetOutputPortWidth(S, DEPTH_PORT_INDEX, getIntParam(S, DEPTH_LENGTH_INDEX) + 1);
    ssSetOutputPortDataType(S, RGB_PORT_INDEX, SS_UINT8);
    ssSetOutputPortDataType(S, DEPTH_PORT_INDEX, SS_SINGLE);

    // INITIALIZE WORK VECTORS
    ssSetNumIWork(S, (int)IWORK_COUNT);
}

static void mdlInitializeSampleTimes(SimStruct *S)
{
    const mxArray *mexPtr = ssGetSFcnParam(S, BLOCK_SAMPLETIME_INDEX);
    double sampleTime = mxGetScalar(mexPtr);

    ssSetSampleTime(S, 0, sampleTime);
    ssSetOffsetTime(S, 0, 0.0);
    ssSetModelReferenceSampleTimeDefaultInheritance(S);
}

// MODEL START - RESOURCE ALLOCATION ----------------------------------------------
#define MDL_START
static void mdlStart(SimStruct *S)
{
    activeSimulinkBlocksCount++; // used to track and free resources later

    // RESOURCE ALLOCATION...
    std::string file = getXmlFilePath(S);
    /* 
        create a unique pointer to instance and assign to mi.
        Had to be done since the instance cannot be movied/copied to another address due to mutex.
        No problems expected as memory is still the same and only the address is put in mi.
        No performance impact expected due to using vector this way (during allocation).
    */
    sd.miInitMutex.lock();
        // expand mi instances
        sd.mi.push_back(make_shared<MujocoModelInstance>());
        int miIndex = sd.mi.size()-1;
        // mi will not be reduced or reordered, so mutex can be unlocked.
    sd.miInitMutex.unlock(); 
    
    // MODEL INIT
    if(sd.mi[miIndex]->initMdl(file, true, false) != 0)
    {
       ssSetLocalErrorStatus(S,"Unable to initialize model in mdlStart");
       return;
    }

    // MODEL DATA INIT
    if(sd.mi[miIndex]->initData() != 0)
    {
       ssSetLocalErrorStatus(S,"Unable to initialize model instance data in mdlStart");
       return;
    }

    {
        // INIT CAMERA RENDER INTERVAL
        const mxArray *paramMx = ssGetSFcnParam(S, CAMERA_SAMPLETIME_INDEX);
        double cameraSampleTime = mxGetScalar(paramMx);
        sd.mi[miIndex]->cameraRenderInterval = cameraSampleTime;
    }

    ssSetIWorkValue(S, MI_IW_IDX, miIndex);

    // VISUALIZATION SETUP
    const mxArray *renderingTypeMx = ssGetSFcnParam(S, RENDERING_INDEX);
    char renderingTypeStr[PARAM_STRING_LIMIT];
    mxGetString(renderingTypeMx, renderingTypeStr, PARAM_STRING_LIMIT-1);

    renderingTypeEnum renderingType;

    if(strcmp(renderingTypeStr, "Local") == 0) renderingType = RENDERING_LOCAL;
    else if(strcmp(renderingTypeStr, "Global") == 0) renderingType = RENDERING_GLOBAL;
    else renderingType = RENDERING_NONE;

    sd.mgInitMutex.lock();
    if(renderingType == RENDERING_LOCAL || renderingType == RENDERING_GLOBAL)
    {
        int mgIndex = 0;
        if(sd.mg.size() == 0 || renderingType == RENDERING_LOCAL)
        {
            sd.mg.push_back(make_shared<MujocoGUI>());
            mgIndex = sd.mg.size()-1;

            guiErrCodes guiStatus = sd.mg[mgIndex]->init(sd.mi[miIndex], MJ_WINDOW);

            {
                // SETUP VSYNC
                const mxArray *paramMx = ssGetSFcnParam(S, VSYNC_INDEX);
                double vsync = mxGetScalar(paramMx);
                sd.mg[mgIndex]->isVsyncOn = (static_cast<int>(vsync) == 1) ? true : false;
            }

            {
                // SETUP ZOOM LEVEL
                const mxArray *paramMx = ssGetSFcnParam(S, ZOOM_LEVEL_INDEX);
                double zoomLevel = mxGetScalar(paramMx);
                sd.mg[mgIndex]->zoomLevel = zoomLevel;
            }

            {
                // SETUP FPS
                const mxArray *paramMx = ssGetSFcnParam(S, VISUAL_FPS_INDEX);
                double fps = mxGetScalar(paramMx);
                std::chrono::milliseconds renderInterval{static_cast<int>(1000.0/fps)};
                sd.mg[mgIndex]->renderInterval = renderInterval;
            }

            if(guiStatus != NO_ERR)
            {
                static std::string err = "Unable to initialize GUI in mdlStart. Error code=" + std::to_string(guiStatus);
                ssSetLocalErrorStatus(S, err.c_str()); // do not pass char array that may go out of its lifetime
                return;
            }
            sd.mg[mgIndex]->addMi(sd.mi[miIndex]); // add to the list 
        }
        else
        {
            sd.mg[mgIndex]->addMi(sd.mi[miIndex]); // add to the list 
        }
        ssSetIWorkValue(S, MG_IW_IDX, mgIndex);
    }
    else
    {
        // do nothing. no rendering to be done
        ssSetIWorkValue(S, MG_IW_IDX, -1);
    }
    sd.mgInitMutex.unlock();

}

#define MDL_UPDATE
static void mdlUpdate(SimStruct *S, int_T tid)
{
    if(!sd.renderingThreadStarted)
    {
        sd.renderingThreadStarted = true;
        sd.renderingThread = std::thread(renderingThreadFcn);
    }

    // progress simulation by 1 time step in discrete time
    int miIndex = ssGetIWorkValue(S, MI_IW_IDX);  

    vector<double> uVec;
    int_T nInputs = ssGetInputPortWidth(S, CONTROL_PORT_INDEX) - 1; // last index is a dummy
    InputRealPtrsType uPtrs = ssGetInputPortRealSignalPtrs(S, CONTROL_PORT_INDEX);
    for (int i = 0; i < nInputs; i++) 
    {
        uVec.push_back((double) *uPtrs[i]);
    }

    auto &miTemp = sd.mi[miIndex]; 

    // Step the simulation by one discrete time step. Outputs (sensors and camera) get reflected in the next step
    miTemp->step(uVec);
}

void renderingThreadFcn()
{
    // should run only after all initialization is done for mg.
    
    // Do opengl or glfw calls only in this thread. Or its gonna crash/error out. 

    // TODO - 
    // Ideally glfw calls is to be made from main thread. In case of linux and windows, 
    //  even using a single thread other than main should work. 
    // But in mac I expect this workflow to break (normal simulation)

    // I am not sure about the thread MATLAB uses to execute this s function

    // MODEL CAMERA INIT
    for(int miIndex=0; miIndex<sd.mi.size(); miIndex++)
    {
        auto &miTemp = sd.mi[miIndex];
        std::lock_guard<std::mutex> mutLock(miTemp->camiMutex);
        
        cameraInterface &camiTemp = miTemp->cami;
        camiTemp.count = miTemp->offscreenCam.size();
        // names are not needed here in s function.

        unsigned long rgbAddr = 0;
        unsigned long depthAddr = 0;
        for(int camIndex = 0; camIndex<camiTemp.count; camIndex++)
        {
            offscreenSize offSize;
            auto guiStatus = sd.mi[miIndex]->offscreenCam[camIndex]->initInThread(&offSize);
            if(guiStatus != NO_ERR)
            {
                std::lock_guard<std::mutex> renderingInitErrLock(sd.renderingInitErrMutex);
                sd.renderingInitErr = guiStatus;
                // STOP SIMULATION HERE ITSELF!
                // lets not stop simulation due to a rendering issue. 
                // throw a warning at the end of simulation
                // TODO - Donot know how to stop simulation here. (that works in both codegen and normal mode)
            }
            camiTemp.size.push_back(offSize);
            camiTemp.rgbAddr.push_back(rgbAddr);
            camiTemp.depthAddr.push_back(depthAddr);
            rgbAddr += 3*offSize.height*offSize.width; // location of next rgb or length of rgb stored so far
            depthAddr += offSize.height*offSize.width;
        }
        camiTemp.rgbLength = rgbAddr;
        camiTemp.depthLength = depthAddr;
    }

    // INIT GUI windows
    for(int index = 0; index<sd.mg.size(); index++)
    {
        auto guiStatus = sd.mg[index]->initInThread();
        if(guiStatus != NO_ERR)
        {
            std::lock_guard<std::mutex> renderingInitErrLock(sd.renderingInitErrMutex);
            sd.renderingInitErr = guiStatus;
            // lets not stop simulation due to a rendering issue. 
            // throw a warning at the end of simulation         
        }
        glfwSetCursorPosCallback(sd.mg[index]->window, mouseMoveCallback);
        glfwSetMouseButtonCallback(sd.mg[index]->window, mouseButtonCallback);
        glfwSetScrollCallback(sd.mg[index]->window, scrollCallback);
    }
    

    // Visualization window and offscreen buffer rendering loop
    while(1)
    {
        // Visualization window(s)
        for(int index=0; index<sd.mg.size(); index++)
        {
            auto duration = std::chrono::steady_clock::now() - sd.mg[index]->lastRenderClockTime;
            if (duration>sd.mg[index]->renderInterval)
            {
                if(sd.mg[index]->loopInThread() == 0) 
                {
                    sd.mg[index]->lastRenderClockTime = std::chrono::steady_clock::now();
                }
            }
        }

        // Offscreen buffers
        for(int miIndex=0; miIndex<sd.mi.size(); miIndex++)
        {   
            auto &miTemp = sd.mi[miIndex];

            if(miTemp->shouldCameraRenderNow == true)
            {
                // if rendering is already done and not consumed, dont do again
                for(int camIndex = 0; camIndex<miTemp->offscreenCam.size(); camIndex++)
                {
                    auto status = miTemp->offscreenCam[camIndex]->loopInThread();
                    if(status == 0)
                    {
                        miTemp->lastRenderTime = miTemp->get_d()->time;
                        miTemp->isCameraDataNew = true; // Used to indicate that a new data is available for copying into blk output
                    }
                }
                miTemp->shouldCameraRenderNow = false;
                miTemp->cameraSync.release();
                
            }
        }
        // If there is nothing to render, donot keep spinning while loop
        if(sd.signalThreadExit == true) break;
    }

    // Release visualization resources
    for(int index=0; index<sd.mg.size(); index++)
    {
        sd.mg[index]->releaseInThread();
    }

    // Release offscreen buffers
    for(int miIndex=0; miIndex<sd.mi.size(); miIndex++)
    {
        for(int camIndex = 0; camIndex<sd.mi[miIndex]->offscreenCam.size(); camIndex++)
        {
            sd.mi[miIndex]->offscreenCam[camIndex]->releaseInThread();
        }
    }
    
    glfwTerminate();
}

static void mdlOutputs(SimStruct *S, int_T tid)
{
    int miIndex = ssGetIWorkValue(S, MI_IW_IDX);
    auto &miTemp = sd.mi[miIndex]; 
    
    // Copy sensors to output
    real_T *y = ssGetOutputPortRealSignal(S, SENSOR_PORT_INDEX);
    int_T ny = ssGetOutputPortWidth(S, SENSOR_PORT_INDEX);
    int_T index = 0;

    auto nSensors = miTemp->si.count;
    for(int_T i=0; i<nSensors; i++)
    {
        vector<double> yVec = miTemp->getSensor(i);
        for(auto elem: yVec)
        {
            y[index] = elem;
            index++;
            // TODO add a check in case index exceeds the allowed limit
        }
    }
    y[index] = static_cast<double>(nSensors); // last element is a dummy to handle empty sensor case

    // Render camera based on the current states. mdlupdate will be called after mdloutputs and update moves the time tk to tk+1
    if(miTemp->offscreenCam.size() != 0)
    {
        double elapsedTimeSinceRender = miTemp->get_d()->time - miTemp->lastRenderTime;
        if( elapsedTimeSinceRender > (miTemp->cameraRenderInterval-0.00001) )
        {
            // maintain camera and physics in sync at required camera sample time
            miTemp->shouldCameraRenderNow = true;
            miTemp->cameraSync.acquire(); // blocking till offscreen buffer is rendered

            // ssPrintf("sim time=%lf & render time=%lf\n", miTemp->get_d()->time, miTemp->lastRenderTime);
        }
    }

    // Copy camera to output
    uint8_T *rgbOut = (uint8_T *) ssGetOutputPortSignal(S, RGB_PORT_INDEX);
    real32_T *depthOut = (real32_T *) ssGetOutputPortSignal(S, DEPTH_PORT_INDEX);
    if(miTemp->isCameraDataNew)
    {
        //avoid unnecessary memcpy. copy only when there is new data. Rest of the time steps, old data will be output
        miTemp->getCameraRGB((uint8_t *) rgbOut);
        miTemp->getCameraDepth((float *) depthOut);
        miTemp->isCameraDataNew = false;
    }
}

static void mdlTerminate(SimStruct *S)
{
    sd.signalThreadExit = true;
    if(sd.renderingThread.joinable()) sd.renderingThread.join();

    std::lock_guard<std::mutex> lockSD (sdMutex);
    activeSimulinkBlocksCount--;
    if(activeSimulinkBlocksCount == 0)
    {
        sd.renderingInitErrMutex.lock();
        if (sd.renderingInitErr != NO_ERR)
        {
            std::string err = "Rendering has failed in initInThread(). Error code is ";
            err += std::to_string(static_cast<int>(sd.renderingInitErr)) + "\n";
            ssWarning(S, err.c_str());
        }
        sd.renderingInitErrMutex.unlock();
        
        sd.deleter();
    }
}

#ifdef  MATLAB_MEX_FILE    /* Is this file being compiled as a MEX-file? */
#include "simulink.c"      /* MEX-file interface mechanism */
#else
#include "cg_sfun.h"       /* Code generation registration function */
#endif
