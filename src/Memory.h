#pragma once

#include "libretro/Core.h"

#include "imgui/imgui.h"

#include <stdio.h>
#include <string>
#include <vector>

#include "imguiext/imgui_memory_editor.h"

class Memory
{
public:
  enum class Platform
  {
    kNes,
    kSnes,
    kMasterSystem,
  };

  bool init(libretro::Core* core);
  void destroy();
  void draw();

  inline Platform getPlatform() const { return _platform; }

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
    unsigned    _id;
    size_t      _start;
    const char* _name;
  };

  bool initWidthMmap(const Block* block);
  bool initWidthMdata(const Block* block);

  bool initNes();
  bool initSnes();
  bool initMasterSystem();

  libretro::Core*     _core;
  bool                _opened;
  std::vector<Region> _regions;
  Platform            _platform;
};
