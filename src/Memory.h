#pragma once

#include "libretro/Core.h"

#include "Platform.h"
#include "imgui/imgui.h"

#include <stdio.h>
#include <string>
#include <map>

#include "imguiext/imgui_memory_editor.h"

class Memory
{
public:
  bool init(libretro::Core* core, Platform platform);
  void destroy();
  void draw();

protected:
  struct Region
  {
    std::string  _description;
    void*        _contents;
    size_t       _size;
    size_t       _baseAddr;
    MemoryEditor _editor;
  };

  struct Block
  {
    unsigned    _memid;
    size_t      _start;
    const char* _identifier;
    const char* _description;
  };

  bool initWidthMmap(const Block* block);
  bool initWidthMdata(const Block* block);

  bool initNES();
  bool initSNES();
  bool initSMS();

  libretro::Core* _core;
  Platform        _platform;
  bool            _opened;

  std::map<std::string, Region> _regions;
};
