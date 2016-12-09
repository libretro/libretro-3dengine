LOCAL_PATH := $(call my-dir)
GLES = 1

include $(CLEAR_VARS)

GIT_VERSION := " $(shell git rev-parse --short HEAD || echo unknown)"
ifneq ($(GIT_VERSION)," unknown")
	LOCAL_CXXFLAGS += -DGIT_VERSION=\"$(GIT_VERSION)\"
endif

LOCAL_MODULE := retro

ifeq ($(TARGET_ARCH),arm)
LOCAL_CFLAGS += -DANDROID_ARM
LOCAL_ARM_MODE := arm
endif

ifeq ($(TARGET_ARCH),x86)
LOCAL_CFLAGS += -DANDROID_X86
endif

ifeq ($(TARGET_ARCH),mips)
LOCAL_CFLAGS += -DANDROID_MIPS
endif

CORE_DIR := ..

include $(CORE_DIR)/Makefile.common

LOCAL_SRC_FILES += $(SOURCES_CXX) $(SOURCES_C)
COMMONFLAGS     := -DHAVE_OPENGLES -DHAVE_OPENGLES2 -DANDROID -DHAVE_RJPEG -DHAVE_RTGA -DHAVE_RBMP -DHAVE_RPNG
LOCAL_CXXFLAGS += -O2 -Wall -ffast-math -fexceptions $(COMMONFLAGS) $(INCFLAGS)
LOCAL_CFLAGS += $(COMMONFLAGS) $(INCFLAGS)
LOCAL_LDLIBS += -lz -llog -lGLESv2

include $(BUILD_SHARED_LIBRARY)

