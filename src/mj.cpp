// Copyright 2022-2023 The MathWorks, Inc.
#include "mj.hpp"
#include <iostream>
#include <stdlib.h>
#include <string.h> 
#include <fstream>

// STATIC AND GLOBALS

std::recursive_mutex glfwMutex; 
// OpenGL is not reentrant (and not thread safe). 
// Lock the graphics library so that no other thread can use opengl.

using std::shared_ptr;

// Aligned malloc - Visual Studio Specific implementation
// #include <malloc.h>
// #define MALLOC(buf, alignment) _aligned_malloc(buf, alignment)
// #define FREE(buf) _aligned_free(buf)

#define MALLOC(buf, alignment) malloc(buf)
#define FREE(buf) free(buf)

// MODEL --------------------------------------------------------------------------
int MujocoModelInstance::initMdl(std::string file, bool shouldInitCam, bool shouldGetCami)
{
    char err[1000] = "err";
    m = mj_loadXML(file.c_str(), 0, err, 1000);
    if (!m) 
    {
        return -1;
    }

    ci = getControlInterface();
    si = getSensorInterface();

    int errCode = 0;
    if(shouldInitCam)
    {
        errCode = initCameras();
        if(shouldGetCami)
        {
            cami = getCameraInterface();
        }
        else
        {
            cami.count = 0;
        }
    }

    return errCode;
}

int MujocoModelInstance::initCameras()
{
    int ncams = m->ncam;
    for(int camIndex=0; camIndex<ncams; camIndex++)
    {
        offscreenCam.push_back(std::make_shared<MujocoGUI>());

        guiErrCodes offscreenStatus = offscreenCam[camIndex]->init(this, MJ_OFFSCREEN);
        if(offscreenStatus != NO_ERR)
        {
            // TODO better error message with code
            return -2;
        }

        offscreenCam[camIndex]->camType = mjCAMERA_FIXED; // only handling fixed type camera now. assuming all cameras in xml as fixed type!
        offscreenCam[camIndex]->camId = camIndex;
    }
    return 0;
}

int MujocoModelInstance::initData()
{
    char err[1000] = "err";
    d = mj_makeData(m);
    if(!d) return -1;
    else return 0;
}

MujocoModelInstance::~MujocoModelInstance()
{
    mj_deleteModel(m);
    mj_deleteData(d);
}

mjModel *MujocoModelInstance::get_m()
{
    return m;
}

mjData *MujocoModelInstance::get_d()
{
    return d;
}

double MujocoModelInstance::getSampleTime()
{
    return m->opt.timestep;
}

sensorInterface MujocoModelInstance::getSensorInterface()
{
    sensorInterface si;
    si.count = m->nsensor;
    
    std::vector<std::string> names;
    for (unsigned index = 0; index < si.count; index++)
    {
        char *namePointer = m->names + m->name_sensoradr[index];
        std::string str(namePointer);
        names.push_back(str);
    }
    si.names = names;

    std::vector<unsigned> dim;
    si.scalarCount = 0;
    for (unsigned index = 0; index < si.count; index++)
    {
        unsigned sensor_dim = m->sensor_dim[index];
        dim.push_back(sensor_dim);
        si.scalarCount += sensor_dim;
    }
    si.dim = dim;

    std::vector<unsigned> addr;
    for (unsigned index = 0; index < si.count; index++)
    {
        unsigned sensor_addr = m->sensor_adr[index];
        addr.push_back(sensor_addr);
    }
    si.addr = addr;
    return si;
}

controlInterface MujocoModelInstance::getControlInterface()
{
    controlInterface ci;
    ci.count = m->nu;
    
    std::vector<std::string> names;
    for (unsigned index = 0; index < ci.count; index++)
    {
        char *namePointer = m->names + m->name_actuatoradr[index];
        std::string str(namePointer);
        names.push_back(str);
    }
    ci.names = names;
    return ci;
}

cameraInterface MujocoModelInstance::getCameraInterface()
{
    // CALL THIS FUNCTION ONLY FROM MAIN THREAD (MAC) OR THE THREAD THAT HANDLES THE REST OF RENDERING GLFW OPENGL WORK
    // Run init and initCameras before running this
    cameraInterface camiTemp;
    camiTemp.count = offscreenCam.size();

    std::vector<std::string> names;
    for(unsigned index=0; index<camiTemp.count; index++)
    {
        char *namePointer = m->names + m->name_camadr[index];
        std::string str(namePointer);
        names.push_back(str);
    }
    camiTemp.names = names;

    unsigned long rgbAddr = 0;
    unsigned long depthAddr = 0;
    for(int index=0; index<camiTemp.count; index++)
    {
        offscreenSize offSize;
        offscreenCam[index]->initInThread(&offSize, true);
        // TODO handle error
        camiTemp.size.push_back(offSize);
        
        camiTemp.rgbAddr.push_back(rgbAddr);
        camiTemp.depthAddr.push_back(depthAddr);
        rgbAddr += 3*offSize.height*offSize.width; // location of next rgb or length of rgb stored so far
        depthAddr += offSize.height*offSize.width;
    }
    camiTemp.rgbLength = rgbAddr;
    camiTemp.depthLength = depthAddr;
    
    return camiTemp;
}

void MujocoModelInstance::step(std::vector<double> u)
{
    // same memory location will be accessed during gui rendering
    dMutex.lock();
    for (unsigned index = 0; index < u.size(); index++)
    {
        d->ctrl[index] = u[index];
    }
    mj_step(m, d);
    dMutex.unlock();
}

std::vector<double> MujocoModelInstance::getSensor(unsigned index)
{
    std::vector<double> sensorData;

    dMutex.lock();
    auto addrStart = si.addr[index];
    auto addrEnd = addrStart + si.dim[index] - 1;
    for (unsigned i = addrStart; i <= addrEnd; i++)
    {
        sensorData.push_back(d->sensordata[i]);
    }
    dMutex.unlock();
    return sensorData;
}

void forcopy(uint8_t *to, uint8_t *from, size_t size)
{
    for(size_t index = 0; index<size; index++)
    {
        to[index] = from[index];
    }
}

size_t MujocoModelInstance::getCameraRGB(uint8_t *buffer)
{
    std::lock_guard<std::mutex> mutLock(camiMutex);
    // if cami.count is 0, either no camera is in model or camera is not initialized
    size_t addr = 0;
    for(int index=0; index<cami.count; index++ )
    {
        size_t rgbArraySize = 3*cami.size[index].height*cami.size[index].width;
        {
            std::lock_guard<std::mutex> mutLock(offscreenCam[index]->camBufferMutex);
            memcpy(buffer+addr, offscreenCam[index]->rgb, rgbArraySize*sizeof(uint8_t));
            // forcopy(buffer+addr, offscreenCam[index]->rgb, rgbArraySize);
        }
        addr += rgbArraySize;
    }
    return addr;
}

void MujocoModelInstance::getCameraDepth(float *buffer)
{
    std::lock_guard<std::mutex> mutLock(camiMutex);
    unsigned long addr = 0;
    for(int index=0; index<cami.count; index++ )
    {
        unsigned long size = cami.size[index].height*cami.size[index].width;
        {
            std::lock_guard<std::mutex> mutLock(offscreenCam[index]->camBufferMutex);
            memcpy(buffer+addr, offscreenCam[index]->depth, size*sizeof(float));
        }
        addr += size;
    }
}

// GUI rendering ------------------------------------------------------------------

guiErrCodes MujocoGUI::init(std::shared_ptr<MujocoModelInstance> mdlInstance, glTarget openglTarget)
{
    return init(mdlInstance.get(), openglTarget);
}

static int err;
static char des[1000];
void glfwFailCallback(int error, const char* description)
{
    err = error;
    strncpy(des, description, sizeof(des));
}

guiErrCodes MujocoGUI::init(MujocoModelInstance* mdlInstance, glTarget openglTarget)
{
    // sets some variables and initializes opengl. Window creation is done in thread init.
    target = openglTarget;

    // sceneAssetModel resources are uploaded to GPU. Note that the different model instances can share the same GPU resources
    sceneAssetModel = mdlInstance;

    if(target == MJ_OFFSCREEN)
    {
        addMi(mdlInstance);
    }
    
    return NO_ERR;
}

guiErrCodes MujocoGUI::initInThread(offscreenSize *offSize, bool stopAtOffScreenSizeCalc)
{
    std::lock_guard<std::recursive_mutex> glLock (glfwMutex); //opengl is not threadsafe or reentrant. lock until it is safe to unlock
    
    glfwSetErrorCallback(&glfwFailCallback);
    if(glfwInit() == 0) 
    {
        exited = true;
        // stop any further opengl work for this object
        return GLFW_INIT_FAILED;
    }

    if(target == MJ_WINDOW)
    {
        glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);
        glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE); 
    }
    else if(target == MJ_OFFSCREEN)
    {
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
        glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_FALSE);
    }
    else
    {
        exited = true;
        return UNKNOWN_TARGET;
    }

    window = glfwCreateWindow(800, 800, "Simulation", NULL, NULL);
    if( !window ) 
    {
        exited = true;
        return WINDOW_CREATION_FAILED;
    }

    glfwMakeContextCurrent(window);

    if(target == MJ_WINDOW) glfwSwapInterval(static_cast<int>(isVsyncOn)); // turn vsync for on screen rendering

    // init all rendering stuff
    mjv_defaultCamera(&cam);
    if (target == MJ_OFFSCREEN)
    {   
        cam.type = camType;
        cam.fixedcamid = camId;
    }
    else
    {
        for(int i=0; i<3; i++)
        {
            cam.lookat[i] = sceneAssetModel->get_m()->stat.center[i];
        }
        
        cam.distance = (sceneAssetModel->get_m()->stat.extent)/zoomLevel;
        cam.type = mjCAMERA_FREE;
    }
 
    mjv_defaultOption(&opt);
    mjv_defaultScene(&scn);
    mjr_defaultContext(&con);

    // upload GPU assets
    mjv_makeScene(sceneAssetModel->get_m(), &scn, 2000);
    mjr_makeContext(sceneAssetModel->get_m(), &con, mjFONTSCALE_100);

    // Set target for the current context and verify the same
    if (target == MJ_OFFSCREEN)
    {
        mjr_setBuffer(mjFB_OFFSCREEN, &con);
        if(con.currentBuffer != mjFB_OFFSCREEN)
        {
            mjv_freeScene(&scn);
            mjr_freeContext(&con);
            glfwDestroyWindow(window);
            exited = true;
            return OFFSCREEN_TARGET_NOT_SUPPORTED;
        }

        // allocate memory for RGB and depth buffers.
        viewport = mjr_maxViewport(&con);
        int W = viewport.width;
        int H = viewport.height;
        if(offSize)
        {
            offSize->height = viewport.height;
            offSize->width  = viewport.width;
            if(stopAtOffScreenSizeCalc)
            {
                mjv_freeScene(&scn);
                mjr_freeContext(&con);
                glfwDestroyWindow(window);
                return NO_ERR;
            }
        }

        // allocate rgb and depth buffers
        rgb = (unsigned char*) MALLOC(3*W*H, 8);
        depth = (float*) MALLOC(sizeof(float)*W*H, 8);

        if( !rgb || !depth )
        {
            if(rgb) FREE(rgb);
            if(depth) FREE(depth);
            mjv_freeScene(&scn);
            mjr_freeContext(&con);
            glfwDestroyWindow(window);

            exited = true;
            return RGBD_BUFFER_ALLOC_FAILED;
        }

    }

    glfwMakeContextCurrent(NULL);
    return NO_ERR;
}

int MujocoGUI::loopInThread()
{
    if(exited == false)
    {
        {
            std::lock_guard<std::recursive_mutex> glLock (glfwMutex);
            if(glfwWindowShouldClose(window)) 
            {
                // If user clicks on X to close the visualization. Close opengl window but let the simulation continue.
                releaseInThread();
                return -1;
            }

            {
                glfwMakeContextCurrent(window);

                // modelInstancesLock.lock();
                for(int index=0; index<mdlInstances.size(); index++)
                {
                    if(index==0) 
                    {
                        refreshScene(mdlInstances[index]);
                    }
                    else 
                    {
                        addGeomsToScene(mdlInstances[index]);
                    }
                    // add remaining dynamic geom locations. 
                    // first model is arbitrarily chosen as the main one.
                }
                // modelInstancesLock.unlock();
                mjr_render(viewport, &scn, &con);
                
                if(target == MJ_WINDOW)
                {
                    // this is a blocking call due to vsync (Update will be done in sync with monitor refresh rate. Doing faster than that can result in screen tearing effects)
                    glfwSwapBuffers(window);
                    glfwPollEvents();
                }
                else
                {   
                    std::lock_guard<std::mutex> mutLock(camBufferMutex);
                    mjr_readPixels(rgb, depth, viewport, &con);
                }
                
                glfwMakeContextCurrent(NULL);
            }
        }
        return 0;
    }
    else
    {
        return -1;
    }
}

void MujocoGUI::releaseInThread()
{
    if(exited == false)
    {
        std::lock_guard<std::recursive_mutex> glLock (glfwMutex);
        if(rgb) FREE(rgb);
        if(depth) FREE(depth);

        glfwMakeContextCurrent(window);
        mjv_freeScene(&scn);
        mjr_freeContext(&con);
        glfwDestroyWindow(window); // automatically detaches if current
        exited = true;
    }
}

MujocoGUI::~MujocoGUI()
{   
    return;
}

void MujocoGUI::refreshScene(MujocoModelInstance* mi)
{
    // refreshScene clears previous scene and adds mi to the new scene

    // retrieve the screen width and height from active window
    if(target != MJ_OFFSCREEN)
    {
        glfwGetFramebufferSize(window, &viewport.width, &viewport.height);
    }
    mi->dMutex.lock(); // lock before using simulation data
    mjv_updateScene(mi->get_m(), mi->get_d(), &opt, NULL, &cam, mjCAT_ALL, &scn);
    mi->dMutex.unlock();
}
void MujocoGUI::addGeomsToScene(MujocoModelInstance* mi)
{
    // adds additional dynamic bodies to the same visualization.
    // This should help with visualizing parallel simulations on the same window
    mi->dMutex.lock();
    mjv_addGeoms(mi->get_m(), mi->get_d(), &opt, NULL, mjCAT_DYNAMIC, &scn);
    mi->dMutex.unlock();
}

void MujocoGUI::addMi(shared_ptr<MujocoModelInstance> mdlInstance)
{
    // model instances that are to be rendered.
    // the first instance is rendered with mjCAT_ALL option
    //  the later instances (if any) are rendered with mjCAT_DYNAMIC
    addMi(mdlInstance.get());
}

void MujocoGUI::addMi(MujocoModelInstance* mdlInstance)
{
    modelInstancesLock.lock();
    mdlInstances.push_back(mdlInstance);
    modelInstancesLock.unlock();
}

// INTERFACES ------------------------------------------------
std::size_t sensorInterface::hash()
{
    using std::to_string;
    std::string str;

    str += "count=" + to_string(count);

    str += "\nnames=";
    for(auto& nameItem: names) str += nameItem + ",";

    str += "\ndim=";
    for(auto& dimItem: dim) str += to_string(dimItem) + ",";

    str += "\naddr=";
    for(auto& addrItem: addr) str += to_string(addrItem) + ",";

    std::hash<std::string> hasher;
    return hasher(str);
}

std::size_t controlInterface::hash()
{
    using std::to_string;
    std::string str;

    str += "count=" + to_string(count);

    str += "\nnames=";
    for(auto& nameItem: names) str += nameItem + ",";

    std::hash<std::string> hasher;
    return hasher(str);
}

std::size_t cameraInterface::hash()
{
    using std::to_string;
    std::string str;

    str += to_string(count);
    for(auto& nameItem: names) str += nameItem;
    for(auto& item: size) 
    {
        str += to_string(item.height);
        str += to_string(item.width);
    }
    // The above uniquely determine camera size properties
    // for(auto& item: depthAddr) str += to_string(item);
    // for(auto& item: rgbAddr) str += to_string(item);

    std::hash<std::string> hasher;
    return hasher(str);
}