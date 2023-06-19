// Minimal C++ interface Wrapper over Mujoco C API

// Refer to MuJoCo website to learn more about the API
// MuJoCo is a trademark of DeepMind

// Copyright 2022-2023 The MathWorks, Inc.

#pragma once
#include "mujoco/mujoco.h"
// #include "glfw3.h"
#include <GLFW/glfw3.h>
#include <mutex>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <atomic>
#include <memory>
#include "semaphore.hpp"

// using namespace std::chrono_literals;

class sensorInterface
{
    public:
    unsigned count;
    unsigned scalarCount; // number of individual data count. imu and rangefinder would give 4 scalars
    std::vector<std::string> names;
    std::vector<unsigned> dim;
    std::vector<unsigned> addr;

    std::size_t hash();
};

class controlInterface
{
    public:
    unsigned count;
    std::vector<std::string> names;

    std::size_t hash();
};

struct offscreenSize
{
    unsigned height;
    unsigned width;
};

class cameraInterface
{
    public:
    unsigned count = 0;
    std::vector<std::string> names;
    std::vector<offscreenSize> size;
    std::vector<unsigned long> rgbAddr;
    std::vector<unsigned long> depthAddr;
    unsigned long rgbLength;
    unsigned long depthLength;

    std::size_t hash();
};

class MujocoGUI;
class MujocoModelInstance
{
    // This class is not designed to be moved or copied
private:
    mjData *d = NULL;
    mjModel *m = NULL;

    int initCameras();

    controlInterface getControlInterface();
    sensorInterface getSensorInterface();
    cameraInterface getCameraInterface();

    // disable copy constructor
    MujocoModelInstance(const MujocoModelInstance &mi);
public:
    std::mutex dMutex; // mutex for model data access 

    // cameras in the model instance
    std::vector<std::shared_ptr<MujocoGUI>> offscreenCam;
    
    // enable default contructor
    MujocoModelInstance() = default;
    ~MujocoModelInstance();

    int initMdl(std::string file, bool shouldInitCam = true, bool shouldGetCami = true);
    int initData();

    // Cache for internal usage
    controlInterface ci;
    sensorInterface si;

    // Camera interface gets initialized in background rendering thread. Protect it with mutex
    cameraInterface cami;
    std::mutex camiMutex;

    double getSampleTime();
    mjModel *get_m();
    mjData *get_d();


    // Camera timing
    double lastRenderTime = 0;
    double cameraRenderInterval = 0.020;
    std::atomic<bool> isCameraDataNew = false;
    binarySemp cameraSync; // semp for syncing main thread and render camera thread
    std::atomic<bool> shouldCameraRenderNow = false;

    void step(std::vector<double> u);
    std::vector<double> getSensor(unsigned index);
    size_t getCameraRGB(uint8_t *buffer);
    void getCameraDepth(float *buffer);
};

enum glTarget
{
    MJ_WINDOW = 0,
    MJ_OFFSCREEN
};
enum guiErrCodes
{
    NO_ERR = 0,
    UNKNOWN_TARGET,
    WINDOW_CREATION_FAILED,
    OFFSCREEN_TARGET_NOT_SUPPORTED,
    RGBD_BUFFER_ALLOC_FAILED,
    GLFW_INIT_FAILED
};

class MujocoGUI
{
    // This class represents a GUI window or an offscreen buffer (used for rendering rgbd cameras)
    // GUI window can be common to multiple model instances (can overlap parallel simulations)

    private:
    mjvOption opt;
    mjrContext con;
    mjrRect viewport = {0, 0, 0, 0};
    glTarget target;

    // adding content to a scene based on current simulation state
    void refreshScene(MujocoModelInstance* mdlInstance);
    void addGeomsToScene(MujocoModelInstance* mdlInstance);
    
    public:
    GLFWwindow *window; // exposed for window callback management
    mjvCamera cam; // exposed for window callback management
    mjvScene scn;
    double zoomLevel = 1.0;

    // rendering asset/scene information
    std::vector<MujocoModelInstance*> mdlInstances;
    MujocoModelInstance* sceneAssetModel;

    // rgb and depth buffers for offscreen rendering
    std::mutex camBufferMutex;
    unsigned char* rgb = nullptr;
    float* depth = nullptr;

    // camera spec
    mjtCamera camType;
    int camId;

    std::atomic<bool> exited = false;
    std::mutex modelInstancesLock;

    // Timing 
    bool isVsyncOn = false;
    std::chrono::microseconds renderInterval;
    std::chrono::time_point<std::chrono::steady_clock> lastRenderClockTime;

    // Initialization
    guiErrCodes init(std::shared_ptr<MujocoModelInstance> mdlInstance, glTarget target);
    guiErrCodes init(MujocoModelInstance* mdlInstance, glTarget target);
    void addMi(std::shared_ptr<MujocoModelInstance> mdlInstance);
    void addMi(MujocoModelInstance* mdlInstance);
    
    // Run these in a single background thread
    guiErrCodes initInThread(offscreenSize *offSize = NULL, bool stopAtOffScreenSizeCalc = false);
    int loopInThread();
    void releaseInThread();

    private:
    // block copy constructor (can lead to double free cases when copied/moved)
    MujocoGUI(const MujocoGUI &g);

    public:
    // Enable default constructior and destructor
    MujocoGUI() = default;
    ~MujocoGUI();
};
