# Simulink Blockset for MuJoCo Simulator 

This repository provides a Simulink&reg; C-MEX S-Function block interface to [MuJoCo&trade; physics engine](https://mujoco.org/).

[![View <File Exchange Title> on File Exchange](https://www.mathworks.com/matlabcentral/images/matlab-file-exchange.svg)](https://www.mathworks.com/matlabcentral/fileexchange/####-file-exchange-title)  

Useful for,
1. Robot simulation (mobile, biomimetic, grippers, robotic arm)
2. Development of autonomous algorithms using classical or machine learning based approaches
3. Camera (RGB, Depth) rendering

## Installation Instructions

### MathWorks&reg; Products (https://www.mathworks.com)

- MATLAB&reg; (required)
- Simulink&reg; (required)
- Computer Vision Toolbox&trade; (optional)
- Robotics System Toolbox&trade; (optional)
- Control System Toolbox&trade; (optional)
- Simulink&reg; Coder&trade; (optional)

MATLAB R2022b or newer recommended. Install MATLAB with the above products and then proceed to setting up MuJoCo blocks.

*Note - You may need to rebuild the S-Function if you are using an older release of MATLAB*.

### Simulink Blockset for MuJoCo

#### Windows&reg;:

- Download the latest release of this repository
- Open MATLAB R2022b. In case you are using a older MATLAB release, please follow the build instructions below to rebuild for a particular MATLAB release
- Run install.m in MATLAB command window. MuJoCo and GLFW libraries will be downloaded and added to MATLAB Path.

#### Ubuntu&reg;:

- Download the latest release of this repository
- Download and install GLFW library from Ubuntu terminal

    `sudo apt update && sudo apt install libglfw3 libglfw3-dev`
- Open MATLAB R2022b. In case you are using a older MATLAB release, please follow the build instructions below to rebuild for a particular MATLAB release
- Run install.m in MATLAB command window. MuJoCo library will be downloaded and added to MATLAB Path.

## Usage
Open examples/gettingStarted.slx model and click run 

If the installation is successful, you should see a pendulum model running in a separate window and camera stream displayed by Video Viewer block (Computer Vision Toolbox)

### Blocks
<img width="250" alt="mjLib" src="https://user-images.githubusercontent.com/8917581/230754094-908a0a52-2c5d-4e8e-bd82-d2dcc553a846.png">

MuJoCo Plant block steps MuJoCo engine, renders visualization window & camera, sets actuator data and outputs sensor readings

It takes an XML (MJCF) as the main block parameter. It auto detects the inputs, sensors, cameras from XML and sets the ports, sample time and other internal configuration.

Inputs can either be a Simulink Bus or vector.

Sensors are output as a Simulink Bus.

RGB and Depth buffers from camera are output as vectors. These can be decoded to Simulink image/matrix using the RGB and Depth Parser blocks.


https://user-images.githubusercontent.com/8917581/230754110-e98b0ed6-05af-416c-9f39-7e5abf562b25.mp4

https://user-images.githubusercontent.com/8917581/230754121-8486a61f-a2db-452c-a943-8682172b4f46.mp4


## Build Instructions (optional)

Steps for building/rebuilding the C-MEX S-Function code. These instructions are only required if you are cloning the repository instead of downloading the release.

### Windows:

- Install one of the following C++ Compiler
  - Microsoft&reg; Visual Studio&reg; 2022 or higher (recommended)
  - (or) [MinGW (GCC 12.2.0 or higher)](https://community.chocolatey.org/packages/mingw)
- Clone this repository
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

    `$ git clone <this_repository.git>`
- Launch MATLAB and open the repository folder. Run the install.m script.
    - `>> install`
- Open tools/ and run the following commands in MATLAB command Windows
    - `>> setupBuild`
    - `>> mex -setup c++`
    - `>> build`


## License

The license is available in the license.txt file within this repository.

## Acknowledgements
Cite this work as,

Manoj Velmurugan.  Simulink Blockset for MuJoCo Simulator (https://github.com/mathworks-robotics/mujoco-simulink-blockset), GitHub. Retrieved March 27, 2023. 


Refer to [MuJoCo repository](https://github.com/deepmind/mujoco) for guidelines on citing MuJoCo physics engine.

The sample codes and API documentation provided for [MuJoCo](https://mujoco.readthedocs.io/en/latest/overview.html) and [GLFW](https://www.glfw.org/documentation) were used as reference material during development.

MuJoCo and GLFW libraries are dynamically linked against the S-Function and are required for running this blockset.

UR5e MJCF XML from [MuJoCo Menagerie](https://github.com/deepmind/mujoco_menagerie/tree/main/universal_robots_ur5e) was used for creating demo videos.

## Community Support

You can post your queries on the [MATLAB&reg; Central&trade; File Exchange](https://www.mathworks.com/matlabcentral/fileexchange/####-file-exchange-title) page.

Copyright 2023 The MathWorks, Inc.
