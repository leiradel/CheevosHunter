# Platform setup
ifeq ($(shell uname -a),)
	EXEEXT=.exe
	SOEXT=.dll
else ifneq ($(findstring MINGW,$(shell uname -a)),)
	EXEEXT=.exe
	SOEXT=.dll
else
	EXEEXT=
	SOEXT=.so
endif

# Toolset setup
CC=gcc
CXX=g++
INCLUDES=-Isrc -Isrc/imgui -Isrc/imgui/examples/sdl_opengl2_example -include debugbreak.h
DEFINES =-DIMGUI_DISABLE_WIN32_DEFAULT_IME_FUNCS -D"IM_ASSERT(x)=do{if(!(x))debug_break();}while(0)"
DEFINES+=-DIMGUIAL_FONTS_PROGGY_TINY -DIMGUIAL_FONTS_FONT_AWESOME -DIMGUIAL_FONTS_MATERIAL_DESIGN -DImFileOpen=fopen
DEFINES+=-DOUTSIDE_SPEEX -DRANDOM_PREFIX=speex -DEXPORT= -D_USE_SSE2 -DFIXED_POINT
DEFINES+=-DPACKAGE=\"cheevoshunter\"
#CCFLAGS=-Wall -O2 $(INCLUDES) $(DEFINES) `sdl2-config --cflags`
CCFLAGS=-Wall -O0 -g $(INCLUDES) $(DEFINES) `sdl2-config --cflags`
CXXFLAGS=$(CCFLAGS) -std=c++11
LDFLAGS=

# ch
CH_OBJS=src/main.o src/ImguiLibretro.o src/CoreInfo.o src/Memory.o src/libretro/BareCore.o src/libretro/Core.o src/dynlib/dynlib.o src/fnkdat/fnkdat.o src/speex/resample.o
CH_LIBS=imgui$(SOEXT) `sdl2-config --libs` -lOpenGL32

# imgui
IMGUI_OBJS=src/imgui/imgui.o src/imgui/imgui_demo.o src/imgui/imgui_draw.o src/imgui/examples/sdl_opengl2_example/imgui_impl_sdl.o
IMGUI_LIBS=-lSDL2.dll -lOpenGL32

# imgui extras
IMGUIEXT_OBJS=src/imguiext/imguial_log.o src/imguiext/imguial_fonts.o src/imguiext/imguifilesystem.o src/imguiext/imguidock.o src/imguiext/imguihelper.o
IMGUIEXT_LIBS=imgui$(SOEXT)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

%.o: %.c
	$(CC) $(CCFLAGS) -c $< -o $@

all: ch$(EXEEXT)

ch$(EXEEXT): $(CH_OBJS) imgui$(SOEXT) imguiext$(SOEXT)
	$(CXX) $(LDFLAGS) -o $@ $+ $(CH_LIBS)

imgui$(SOEXT): $(IMGUI_OBJS)
	$(CXX) -shared $(LDFLAGS) -o $@ $(IMGUI_OBJS) $(IMGUI_LIBS)

imguiext$(SOEXT): imgui$(SOEXT) $(IMGUIEXT_OBJS)
	$(CXX) -shared $(LDFLAGS) -o $@ $(IMGUIEXT_OBJS) $(IMGUIEXT_LIBS)

clean:
	rm -f ch$(EXEEXT) $(CH_OBJS) imgui$(SOEXT) $(IMGUI_OBJS) imguiext$(SOEXT) $(IMGUIEXT_OBJS)

.PHONY: clean
