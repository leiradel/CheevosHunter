#include "Memory.h"

#include "imguiext/imguial_fonts.h"

#include <stdint.h>

enum
{
  kSnes9x        = 0xd17b0bafU, // Snes9x
  kSnes9x2005    = 0x39affeb6U, // Snes9x 2005
  kSnes9x2010    = 0x39affed2U, // Snes9x 2010
  kBsnes         = 0x0f2d5280U, // bsnes
  kBSNES         = 0x0f1b3a00U, // bSNES
  kBsnesMercury  = 0x48965794U, // bsnes-mercury
  kMednafenBSNES = 0x28f2d47eU, // Mednafen bSNES
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
  case kSnes9x:
  case kSnes9x2005:
  case kSnes9x2010:
  case kBsnes:
  case kBSNES:
  case kBsnesMercury:
  case kMednafenBSNES: return initSnes();

  default: return false;
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

bool Memory::initWidthMmap(const Block* block)
{
  const struct retro_memory_map* mmap = _core->getMemoryMap();

  if (mmap == NULL || mmap->num_descriptors == 0)
  {
    return false;
  }

  for (int i = 0; block->_name != NULL; i++, block++)
  {
    const struct retro_memory_descriptor* desc = mmap->descriptors;
    bool found = false;

    for (unsigned j = 0; j < mmap->num_descriptors; j++, desc++)
    {
      if (desc->start == block->_start)
      {
        found = true;
        break;
      }
    }

    if (!found)
    {
      _regions.clear();
      return false;
    }

    Region region;

    region._description = ICON_FA_MICROCHIP;
    region._description.append(" ");
    region._description.append(block->_name);

    region._contents = desc->ptr;
    region._size     = desc->len;
    region._baseAddr = desc->start;

    region._editor.Open = false;

    _regions.push_back(region);
  }

  return true;
}

bool Memory::initWidthMdata(const Block* block)
{
  for (int i = 0; block->_name != NULL; i++, block++)
  {
    void* contents = _core->getMemoryData(block->_id);

    if (contents == NULL)
    {
      _regions.clear();
      return false;
    }

    Region region;

    region._description = ICON_FA_MICROCHIP;
    region._description.append(" ");
    region._description.append(block->_name);

    region._contents = contents;
    region._size     = _core->getMemorySize(block->_id);
    region._baseAddr = block->_start;

    region._editor.Open = false;

    _regions.push_back(region);
  }

  return true;
}

/*
[DEBUG] retro_memory_map
[DEBUG]   ndx flags  ptr              offset   start    select   disconn  len      addrspace
[DEBUG]     1 M1A1bc 0000000048ecef70 00000000 007E0000 00FE0000 00000000 00020000 
[DEBUG]     2 M1A1bc 0000000048f29fd0 00000000 00700000 00708000 00000000 00000800 
[DEBUG]     3 M1A1bC 00000000494dc040 00000000 00C00000 00C00000 00008000 00080000 
[DEBUG]     4 M1A1bC 00000000494dc040 00000000 00808000 00C08000 00008000 00080000 
[DEBUG]     5 M1A1bC 00000000494dc040 00000000 00400000 00C00000 00008000 00080000 
[DEBUG]     6 M1A1bC 00000000494dc040 00000000 00008000 00C08000 00008000 00080000 
[DEBUG]     7 M1A1bc 0000000048ecef70 00000000 00000000 0040E000 00000000 00002000 
*/
static const Memory::Block s_snesBlocks[] =
{
  {RETRO_MEMORY_SYSTEM_RAM, 0x000000, "Work RAM"},
  {RETRO_MEMORY_SAVE_RAM,   0x700000, "Save RAM"},
  {0,                       0x000000, NULL}
};

bool Memory::initSnes()
{
  _platform = Platform::kSnes;

  // Snes9x
  bool ok = initWidthMmap(s_snesBlocks);
  // Snes9x 2010
  ok = ok || initWidthMdata(s_snesBlocks);

  return ok;
}
