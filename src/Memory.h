#pragma once

#include "libretro/CoreManager.h"

#include "imgui/imgui.h"

#include "Snapshot.h"

#include <stdio.h>
#include <string>
#include <vector>

class Memory
{
public:
  bool init(libretro::CoreManager* core);
  void destroy();
  void draw(bool running);

  void reset();

  Snapshot click() const;

protected:
  struct Region
  {
    std::string name;
    uint32_t address;
    void* data;
    size_t size;
  };

  static void asMemorySize(char* str, size_t size, size_t numBytes);
  void addMemory(unsigned id, char const* name);

  void drawMemory(bool running);
  void drawFilters();

  libretro::CoreManager* _core;
  std::vector<Region> _map;
  int _selected;
};
