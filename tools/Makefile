# Use gmake to run this makefile

# if you get permission error while building mex. Run >>clear mex in MATLAB command window

# Copyright 2022-2023 The MathWorks, Inc.

# USER CONFIG STARTS ---------------------------------------------------------------------------

ifeq ($(OS),Windows_NT)
## WINDOWS LOCAL BUILD CONFIG
include ./win64.mk
MEX="$(ML_ROOT)/bin/mex.bat"
else
## LINUX LOCAL BUILD CONFIG
include ./glnxa64.mk
MEX="$(ML_ROOT)/bin/mex"
endif

GLFW_IMPORTLIB_PATH=$(SET_GLFW_IMPORTLIB_PATH)
# USER CONFIG ENDS ---------------------------------------------------------------------------

# IN CASE MJ_PATH is empty, it is local build. else it is gitlab-ci build
ifeq ($(MJ_PATH),)
MJ_PATH:=$(SET_MJ_PATH)
MEX_SETUP:=$(SET_MEX_SETUP)
else
MEX_SETUP:=
endif

ifeq ($(ML_ROOT),)
ML_ROOT:=$(SET_ML_ROOT)
endif

ifeq ($(ARCH),)
ARCH:=$(SET_ARCH)
endif

# OUTPUT
OUT_DIR=../blocks

# COMPILER INCLUDE AND SRC
SRC_PATH=../src
INC_PATH=-I$(MJ_PATH)/include/mujoco -I$(MJ_PATH)/include -I$(SRC_PATH) $(SET_GLFW_INC_PATH)
SRC_COMMON=$(SRC_PATH)/mj.cpp

SRC_FILES := $(wildcard $(SRC_PATH)/*.cpp)
SRC_FILES := $(filter-out $(SRC_COMMON), $(SRC_FILES))

TARGET_FILES := $(subst .cpp,,$(SRC_FILES))
TARGET_FILES := $(subst $(SRC_PATH)/,,$(TARGET_FILES))

# LINKER AND CPP CONFIG
LINKER_OBJ_WIN_COMMON=$(GLFW_IMPORTLIB_PATH)/glfw3dll.lib $(MJ_PATH)/lib/$(MJ_LIB_NAME).lib
ifeq ($(WIN_COMPILER), MINGW)
# LINKER_OBJ_WIN=-ldl $(LINKER_OBJ_WIN_COMMON)
LINKER_OBJ_WIN=$(LINKER_OBJ_WIN_COMMON)
CPP_FLAG=CXXFLAGS="-std=c++17"
LINKFLAGS=LINKFLAGS="-Wl,--no-undefined"
endif
ifeq ($(WIN_COMPILER), MSVCPP)
LINKER_OBJ_WIN=$(LINKER_OBJ_WIN_COMMON)
CPP_FLAG=COMPFLAGS="/EHsc /std:c++17"
LINKFLAGS=
endif

LINKER_OBJ_LINUX=-L$(MJ_PATH)/lib -L/usr/local/lib -l$(MJ_LIB_NAME) -lGL -lglfw

# MEX COMMAND
BUILD_CMD_COMMON=$(MEX) $(SRC_COMMON) $(INC_PATH) -outdir $(OUT_DIR)

ifeq ($(ARCH), win64)
BUILD_CMD=$(BUILD_CMD_COMMON) $(CPP_FLAG) $(LINKER_OBJ_WIN) $(LINKFLAGS)
endif

ifeq ($(ARCH), glnxa64)
BUILD_CMD=$(BUILD_CMD_COMMON) CXXFLAGS="-std=c++17 -fPIC" $(LINKER_OBJ_LINUX) LINKFLAGS="-Wl,--no-undefined" LDFLAGS="\$$LDFLAGS -Wl,-rpath,\\\$$ORIGIN"
endif

DEBUG_FLAG=-g
setup:
	$(MEX_SETUP)

$(TARGET_FILES):
	echo --------------------------------------;
	$(BUILD_CMD) $(SRC_PATH)/$@.cpp $(DEBUG_FLAG) -output $@

build: $(TARGET_FILES)
.PHONY: debug build setup $(TARGET_FILES)
