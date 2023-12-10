# Simulink Blockset for MuJoCo Simulator 

This repository provides a Simulink&reg; C-MEX S-Function block interface to the [MuJoCo&trade; physics engine](https://mujoco.org/).

[![View <File Exchange Title> on File Exchange](https://www.mathworks.com/matlabcentral/images/matlab-file-exchange.svg)](https://www.mathworks.com/matlabcentral/fileexchange/128028-mujoco-simulink-blockset)

Useful for,
1. Robot simulation (mobile, biomimetics, grippers, robotic arm)
2. Development of autonomous algorithms
3. Camera (RGB, Depth) rendering

## Installation Instructions

### MathWorks&reg; Products (https://www.mathworks.com)

- MATLAB&reg; (required)
- Simulink&reg; (required)
- Computer Vision Toolbox&trade; (optional)
- Robotics System Toolbox&trade; (optional)
- Control System Toolbox&trade; (optional)
- Simulink&reg; Coder&trade; (optional)

MATLAB R2022b or newer is recommended. Install MATLAB with the above products and then proceed to set up MuJoCo blocks.

*Note - You may need to rebuild the S-Function if you are using an older release of MATLAB*.

### Simulink Blockset for MuJoCo
    
- (Linux users) Install GLFW library from Ubuntu terminal
    
    `sudo apt update && sudo apt install libglfw3 libglfw3-dev`
- Download the latest release of this repository (MATLAB Toolbox - MLTBX file)
- Open MATLAB R2022b or higher and install the downloaded MATLAB Toolbox file
- Run the setup function packaged in the toolbox. MuJoCo (and GLFW for Windows users) library is downloaded and added to MATLAB path.
    
    `>>mujoco_interface_setup`
- (Linux users) The default pathdef.m is likely not saveable in Linux. Save pathdef.m to new location as given in this [MATLAB answer](https://www.mathworks.com/matlabcentral/answers/1653435-how-to-use-savepath-for-adding-path-to-pathdef-m-in-linux).
    `savepath ~/Documents/MATLAB/pathdef.m`
    
## Usage
`>>mj_gettingStarted`
    
Open the example model and run it in normal simulation mode.

If the installation is successful, you should see a pendulum model running in a separate window and camera streams displayed by Video Viewer blocks (Computer Vision Toolbox).

*A dedicated graphics card is recommended for the best performance. Disable Video Viewer blocks if the model runs slow*

*(Linux users) - In case MATLAB crashes, it may be due to a glibc bug. Please follow this [bug report](https://www.mathworks.com/support/bugreports/2632298) for a workaround!*

### Blocks
<img width="400" alt="mjLib" src="https://user-images.githubusercontent.com/8917581/230754094-908a0a52-2c5d-4e8e-bd82-d2dcc553a846.png">

MuJoCo Plant block steps MuJoCo engine, renders visualization window & camera, sets actuator data, and outputs sensor readings

It takes an XML (MJCF) as the main block parameter. It auto-detects the inputs, sensors, and cameras from XML to configure the block ports and sample time.

Inputs can either be a Simulink Bus or a vector.

Sensors are output as a Simulink Bus.

RGB and Depth buffers from cameras are output as vectors. These can be decoded to Simulink image/matrix using the RGB and Depth Parser blocks.


https://user-images.githubusercontent.com/8917581/230754110-e98b0ed6-05af-416c-9f39-7e5abf562b25.mp4

https://user-images.githubusercontent.com/8917581/230754121-8486a61f-a2db-452c-a943-8682172b4f46.mp4


## Build Instructions (optional)

Steps for building/rebuilding the C-MEX S-Function code. These instructions are only required if you are cloning the repository instead of downloading the release.

### Windows:

- Install one of the following C++ Compiler
  - Microsoft&reg; Visual Studio&reg; 2022 or higher (recommended)
  - (or) [MinGW (GCC 12.2.0 or higher)](https://community.chocolatey.org/packages/mingw)
- Clone this repository
    
    `$ git clone git@github.com:mathworks-robotics/mujoco-simulink-blockset.git`
- Launch MATLAB and open the repository folder
    - `>> install`
- Open tools/ 
    - Open setupBuild.m. In case you are using MinGW compiler, edit the file and set selectedCompilerWin to "MINGW".
    - `>> setupBuild`
    - `>> mex -setup c++`
    - `>> build`

### Ubuntu

- Install the tools required for compiling the S-Function

    `$ sudo apt update && sudo apt install build-essential git libglfw3 libglfw3-dev `
- Clone this repository

    `$ git clone git@github.com:mathworks-robotics/mujoco-simulink-blockset.git`
- Launch MATLAB and open the repository folder. Run the install.m script.
    - `>> install`
- Open tools/ and run the following commands in MATLAB command Windows
    - `>> setupBuild`
    - `>> mex -setup c++`
    - `>> build`

## Tips and Tricks
- ***Code generation*** - The MuJoCo Plant block supports code generation (Simulink Coder) and monitor and tune for host target. Refer to mj_monitorTune.slx for more info.
- ***Performance improvement*** - In case you want to reduce the mask initialization overhead, you can directly use the underlying S-Function. Select the MuJoCo Plant block and Ctrl+U to look under the subsystem mask. Make sure to call the initialization functions (whenever the MJCF XML model changes).

## Bugs/Workarounds
### GLFW 

In case MATLAB crashes while running getting started model and you see the following lines in stack trace,

`#10 0x00007fdaf8619f40 in glfwCreateWindow () at /lib/x86_64-linux-gnu/libglfw.so.3`

`#11 0x00007fdaf8675c4d in MujocoGUI::initInThread(offscreenSize*, bool)`,

Updating glfw could fix the issue.

Building glfw from source ([glfw main - commit id](https://github.com/glfw/glfw/tree/46cebb5081820418f2a20f3e90b07f9b1bd44b42)) and installing fixed this issue for me,
- sudo apt remove libglfw3 libglfw3-dev # Remove existing glfw
- mkdir ~/glfwupdated
- cd ~/gflwupdated
- git clone git@github.com:glfw/glfw.git
- sudo apt install cmake-qt-gui
- cmake-gui
- Set the source directory to the root of cloned repo
- mkdir build in the cloned repo
- Set the build directory
- In Cmake Gui settings - Select "BUILD_SHARED_LIBS" as well
- Configure and then generate
- Open a terminal in the build directory and run $make in terminal
- Once build goes through without any error, run $sudo make install in terminal
- sudo ldconfig (to refresh linker cache)
- Follow the build instructions for mujoco-simulink-blockset

## License

The license is available in the license.txt file within this repository.

## Acknowledgments
Cite this work as,

Manoj Velmurugan.  Simulink Blockset for MuJoCo Simulator (https://github.com/mathworks-robotics/mujoco-simulink-blockset), GitHub. Retrieved date. 


Refer to the [MuJoCo repository](https://github.com/deepmind/mujoco) for guidelines on citing the MuJoCo physics engine.

The sample codes and API documentation provided for [MuJoCo](https://mujoco.readthedocs.io/en/latest/overview.html) and [GLFW](https://www.glfw.org/documentation) were used as reference material during development.

MuJoCo and GLFW libraries are dynamically linked against the S-Function and are required for running this blockset.

UR5e MJCF XML from [MuJoCo Menagerie](https://github.com/deepmind/mujoco_menagerie/tree/main/universal_robots_ur5e) was used for creating demo videos.

## Community Support

<!--- You can post your queries on the [MATLAB&reg; Central&trade; File Exchange](https://www.mathworks.com/matlabcentral/fileexchange/####-file-exchange-title) page. --->

You can post your queries in the discussions section.

Copyright 2023 The MathWorks, Inc.
