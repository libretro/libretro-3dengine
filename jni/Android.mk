LOCAL_PATH := $(call my-dir)

CORE_DIR := $(LOCAL_PATH)/..

GLES     := 1
INCFLAGS :=

include $(CORE_DIR)/Makefile.common

COREFLAGS := -DHAVE_OPENGLES -DHAVE_OPENGLES2 -DANDROID -DHAVE_RJPEG -DHAVE_RTGA -DHAVE_RBMP -DHAVE_RPNG -DINLINE="inline" $(INCFLAGS)

GIT_VERSION := " $(shell git rev-parse --short HEAD || echo unknown)"
ifneq ($(GIT_VERSION)," unknown")
	COREFLAGS += -DGIT_VERSION=\"$(GIT_VERSION)\"
endif

include $(CLEAR_VARS)
LOCAL_MODULE       := retro
LOCAL_SRC_FILES    := $(SOURCES_CXX) $(SOURCES_C)
LOCAL_CFLAGS       := $(COREFLAGS)
LOCAL_CXXFLAGS     := -Wall -ffast-math $(COREFLAGS)
LOCAL_CPP_FEATURES := exceptions
LOCAL_LDFLAGS      := -Wl,-version-script=$(CORE_DIR)/link.T
LOCAL_LDLIBS       := -lz -llog -lGLESv2

# armv5 clang workarounds
ifeq ($(TARGET_ARCH_ABI),armeabi)
	LOCAL_LDLIBS   += -latomic
endif

include $(BUILD_SHARED_LIBRARY)
