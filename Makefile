
ifneq ($(EMSCRIPTEN),)
   platform = emscripten
endif

ifeq ($(platform),)
platform = unix
ifeq ($(shell uname -a),)
   platform = win
else ifneq ($(findstring MINGW,$(shell uname -a)),)
   platform = win
else ifneq ($(findstring Darwin,$(shell uname -a)),)
   platform = osx
else ifneq ($(findstring win,$(shell uname -a)),)
   platform = win
endif
endif

PKG_CONFIG = pkg-config

TARGET_NAME := instancingviewer_camera

ifneq (,$(findstring unix,$(platform)))
   TARGET := $(TARGET_NAME)_libretro.so
   fpic := -fPIC
   SHARED := -shared -Wl,--version-script=link.T -Wl,--no-undefined
ifneq (,$(findstring gles,$(platform)))
   GLES = 1
else
   GL_LIB := -lGL
endif
else ifneq (,$(findstring osx,$(platform)))
   TARGET := $(TARGET_NAME)_libretro.dylib
   fpic := -fPIC -mmacosx-version-min=10.6
   SHARED := -dynamiclib
   GL_LIB := -framework OpenGL
   DEFINES += -DOSX
   CFLAGS += $(DEFINES)
   CXXFLAGS += $(DEFINES)
   INCFLAGS = -Iinclude/compat
else ifneq (,$(findstring armv,$(platform)))
   CC = gcc
   CXX = g++
   TARGET := $(TARGET_NAME)_libretro.so
   fpic := -fPIC
   SHARED := -shared -Wl,--version-script=link.T -Wl,--no-undefined
   CXXFLAGS += -I.
   LIBS := -lz
ifneq (,$(findstring gles,$(platform)))
   GLES := 1
else
   GL_LIB := -lGL
endif
ifneq (,$(findstring cortexa8,$(platform)))
   CXXFLAGS += -marm -mcpu=cortex-a8
else ifneq (,$(findstring cortexa9,$(platform)))
   CXXFLAGS += -marm -mcpu=cortex-a9
endif
   CXXFLAGS += -marm
ifneq (,$(findstring neon,$(platform)))
   CXXFLAGS += -mfpu=neon
   HAVE_NEON = 1
endif
ifneq (,$(findstring softfloat,$(platform)))
   CXXFLAGS += -mfloat-abi=softfp
else ifneq (,$(findstring hardfloat,$(platform)))
   CXXFLAGS += -mfloat-abi=hard
endif
   CXXFLAGS += -DARM
else ifeq ($(platform), ios)
   TARGET := $(TARGET_NAME)_libretro_ios.dylib
   GLES := 1
   SHARED := -dynamiclib
   GL_LIB := -framework OpenGLES
   CC = clang -arch armv7 -isysroot $(IOSSDK) -miphoneos-version-min=5.0
   CXX = clang++ -arch armv7 -isysroot $(IOSSDK) -miphoneos-version-min=5.0
   DEFINES += -DIOS
   CFLAGS += $(DEFINES) -miphoneos-version-min=5.0
   CXXFLAGS += $(DEFINES) -miphoneos-version-min=5.0
   INCFLAGS = -Iinclude/compat
else ifeq ($(platform), qnx)
   TARGET := $(TARGET_NAME)_libretro_qnx.so
   fpic := -fPIC
   SHARED := -lcpp -lm -shared -Wl,-version-script=link.T -Wl,-no-undefined
   CXX = QCC -Vgcc_ntoarmv7le_cpp
   AR = QCC -Vgcc_ntoarmv7le
   GLES = 1
   INCFLAGS = -Iinclude/compat
   LIBS := -lz
else ifeq ($(platform), emscripten)
   TARGET := $(TARGET_NAME)_libretro_emscripten.bc
   GLES := 1
else
   CC = gcc
   TARGET := $(TARGET_NAME)_libretro.dll
   SHARED := -shared -static-libgcc -static-libstdc++ -s -Wl,--version-script=link.T -Wl,--no-undefined
   GL_LIB := -L. -lopengl32
   CXXFLAGS += -DGLEW_STATIC
endif

CFLAGS += -std=gnu99

ifeq ($(DEBUG), 1)
   CXXFLAGS += -O0 -g
   CFLAGS += -O0 -g
else ifeq ($(platform), emscripten)
   CXXFLAGS += -O2
   CFLAGS += -O2
else
   CXXFLAGS += -O3
   CFLAGS += -O3
endif

OBJECTS := libretro.o glsym.o rpng.o
CXXFLAGS += -Wall $(fpic)
CFLAGS += -Wall $(fpic)
CXXFLAGS += $(INCFLAGS)

LIBS += -lz
ifeq ($(GLES), 1)
   CXXFLAGS += -DGLES
ifeq ($(platform), ios)
   LIBS += $(GL_LIB)
else
   LIBS += -lGLESv2
endif
else
   LIBS += $(GL_LIB)
endif

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(fpic) $(SHARED) $(INCLUDES) -o $@ $(OBJECTS) $(LIBS) -lm

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $<

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJECTS) $(TARGET)

.PHONY: clean


