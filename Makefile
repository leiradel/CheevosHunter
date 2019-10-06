# Platform setup
ifeq ($(shell uname -a),)
	SOEXT=.dll
	LIBS=-lOpenGL32
else ifneq ($(findstring MINGW,$(shell uname -a)),)
	SOEXT=.dll
	LIBS=-lOpenGL32
else
	SOEXT=.so
	LIBS=-lGL -ldl
endif

# Toolset setup
CC=gcc
CXX=g++
INCLUDES=-Isrc -Isrc/imgui -Isrc/imgui/examples -Isrc/lua-5.3.5/src -include debugbreak.h
DEFINES =-DIMGUI_DISABLE_WIN32_DEFAULT_IME_FUNCS -D"IM_ASSERT(x)=do{if(!(x))debug_break();}while(0)"
DEFINES+=-DIMGUIAL_FONTS_PROGGY_TINY -DIMGUIAL_FONTS_FONT_AWESOME -DIMGUIAL_FONTS_MATERIAL_DESIGN
DEFINES+=-DOUTSIDE_SPEEX -DRANDOM_PREFIX=speex -DEXPORT= -D_USE_SSE2 -DFIXED_POINT
DEFINES+=-DPACKAGE=\"cheevoshunter\"
#CCFLAGS=-Wall -O2 $(INCLUDES) $(DEFINES) `sdl2-config --cflags`
CCFLAGS=-Wall -O0 -g $(INCLUDES) $(DEFINES) `sdl2-config --cflags`
CXXFLAGS=$(CCFLAGS) -std=c++11 -fPIC
LDFLAGS=

# lua
LUA_OBJS=\
	src/lua-5.3.5/src/lapi.o src/lua-5.3.5/src/lcode.o src/lua-5.3.5/src/lctype.o src/lua-5.3.5/src/ldebug.o \
	src/lua-5.3.5/src/ldo.o src/lua-5.3.5/src/ldump.o src/lua-5.3.5/src/lfunc.o src/lua-5.3.5/src/lgc.o src/lua-5.3.5/src/llex.o \
	src/lua-5.3.5/src/lmem.o src/lua-5.3.5/src/lobject.o src/lua-5.3.5/src/lopcodes.o src/lua-5.3.5/src/lparser.o \
	src/lua-5.3.5/src/lstate.o src/lua-5.3.5/src/lstring.o src/lua-5.3.5/src/ltable.o src/lua-5.3.5/src/ltm.o \
	src/lua-5.3.5/src/lundump.o src/lua-5.3.5/src/lvm.o src/lua-5.3.5/src/lzio.o src/lua-5.3.5/src/lauxlib.o \
	src/lua-5.3.5/src/lbaselib.o src/lua-5.3.5/src/lbitlib.o src/lua-5.3.5/src/lcorolib.o src/lua-5.3.5/src/ldblib.o \
	src/lua-5.3.5/src/liolib.o src/lua-5.3.5/src/lmathlib.o src/lua-5.3.5/src/loslib.o src/lua-5.3.5/src/lstrlib.o \
	src/lua-5.3.5/src/ltablib.o src/lua-5.3.5/src/lutf8lib.o src/lua-5.3.5/src/loadlib.o src/lua-5.3.5/src/linit.o

# ch
CH_OBJS=\
	src/main.o src/ImguiLibretro.o src/CoreInfo.o src/Memory.o src/Set.o src/Snapshot.o \
	src/libretro/Core.o src/libretro/CoreManager.o \
	src/components/Audio.o src/components/Input.o src/components/Video.o \
	src/dynlib/dynlib.o src/fnkdat/fnkdat.o src/speex/resample.o

# imgui
IMGUI_OBJS=\
	src/imgui/imgui.o src/imgui/imgui_widgets.o src/imgui/imgui_demo.o src/imgui/imgui_draw.o \
	src/imgui/examples/imgui_impl_sdl.o src/imgui/examples/imgui_impl_opengl2.o

# imgui extras
IMGUIEXT_OBJS=\
	src/imguiext/imguial_fonts.o src/imguiext/imguifilesystem.o \
	src/imguiext/imguial_term.o

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

%.o: %.c
	$(CC) $(CCFLAGS) -c $< -o $@

all: ch

ch: $(CH_OBJS) $(IMGUI_OBJS) $(IMGUIEXT_OBJS) $(LUA_OBJS)
	$(CXX) $(LDFLAGS) -o $@ $+ $(CH_LIBS) `sdl2-config --libs` $(LIBS)

#imgui$(SOEXT): $(IMGUI_OBJS)
#	$(CXX) -shared $(LDFLAGS) -o $@ $(IMGUI_OBJS) $(IMGUI_LIBS)

#imguiext$(SOEXT): imgui$(SOEXT) $(IMGUIEXT_OBJS)
#	$(CXX) -shared $(LDFLAGS) -o $@ $(IMGUIEXT_OBJS) $(IMGUIEXT_LIBS)

clean:
	rm -f ch $(CH_OBJS) $(IMGUI_OBJS) $(IMGUIEXT_OBJS) $(LUA_OBJS)

.PHONY: clean
