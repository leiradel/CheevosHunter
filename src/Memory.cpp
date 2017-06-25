#include "Memory.h"

#include "imguiext/imguial_fonts.h"

#include <stdint.h>

enum
{
  kSnes9x = 0xd17b0bafU, // snes9x_libretro.dll version 1.53 2d579c6
};

static uint32_t djb2(const char* str)
{
  uint32_t hash = 5381;                
                                       
  while (*str != 0)                       
  {                                    
    hash = hash * 33 + (uint8_t)*str++;
  }                                    
                                       
  return hash;
}

bool Memory::init(libretro::Core* core)
{
  _core = core;
  _opened = true;

  const struct retro_system_info* info = core->getSystemInfo();

  switch (djb2(info->library_name))
  {
  case kSnes9x: return initSnes9x();
  default:      return false;
  }
}

void Memory::destroy()
{
  _regions.clear();
}

void Memory::draw()
{
  for (auto it = _regions.begin(); it != _regions.end(); ++it)
  {
    Region* region = &*it;

    region->_editor.Draw(region->_description.c_str(), (unsigned char*)region->_contents, region->_size, region->_baseAddr);
  }
}

bool Memory::findMemory(Region* region, size_t start)
{
  const struct retro_memory_map* mmap = _core->getMemoryMap();
  const struct retro_memory_descriptor* desc = mmap->descriptors;

  for (unsigned i = 0; i < mmap->num_descriptors; i++, desc++)
  {
    if (desc->start == start)
    {
      region->_contents = desc->ptr;
      region->_size     = desc->len;
      region->_baseAddr = start;

      return true;
    }
  }

  return false;
}

bool Memory::initSnes9x()
{
  /*
  [DEBUG] retro_memory_map
  [DEBUG]   ndx flags  ptr              offset   start    select   disconn  len      addrspace
  [DEBUG]     1 M1A1bc 0000000048ecef70 00000000 007E0000 00FE0000 00000000 00020000 
  [DEBUG]     1 M1A1bc 0000000048f29fd0 00000000 00700000 00708000 00000000 00000800 
  [DEBUG]     1 M1A1bC 00000000494dc040 00000000 00C00000 00C00000 00008000 00080000 
  [DEBUG]     1 M1A1bC 00000000494dc040 00000000 00808000 00C08000 00008000 00080000 
  [DEBUG]     1 M1A1bC 00000000494dc040 00000000 00400000 00C00000 00008000 00080000 
  [DEBUG]     1 M1A1bC 00000000494dc040 00000000 00008000 00C08000 00008000 00080000 
  [DEBUG]     1 M1A1bc 0000000048ecef70 00000000 00000000 0040E000 00000000 00002000 
  */

  static const struct { size_t start; const char* name; } regions[] =
  {
    { 0x000000, "Work RAM" },
    { 0x700000, "Save RAM" },
    { 0x000000, NULL }
  };

  _platform = Platform::kSnes;

  for (int i = 0; regions[i].name != NULL; i++)
  {
    Region region;

    if (findMemory(&region, regions[i].start))
    {
      region._description = ICON_FA_MICROCHIP;
      region._description.append(" ");
      region._description.append(regions[i].name);

      region._editor.Open = false;

      _regions.push_back(region);
    }
    else
    {
      _regions.clear();
      return false;
    }
  }

  return true;
}
