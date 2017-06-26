#include "Memory.h"

#include "imguiext/imguial_fonts.h"

#include <stdint.h>

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
  case 0x7c94ae0dU: // bnes
  case 0xc3eefa9bU: // emux (nes)
  case 0xb00bd8c2U: // FCEUmm
  case 0xeb2f41e8U: // Nestopia
  case 0x670e8ee8U: // QuickNES - this is the only core that implements one of the required interfaces
    return initNes();

  case 0x0f2d5280U: // bsnes
  case 0x0f1b3a00U: // bSNES
  case 0x48965794U: // bsnes-mercury
  case 0xd17b0bafU: // Snes9x
  case 0x39affeb6U: // Snes9x 2005
  case 0x39affed2U: // Snes9x 2010
  case 0x28f2d47eU: // Mednafen bSNES
    return initSnes();
  
  case 0x2ce692d6U: // Genesis Plus GX
  case 0x0cc11b6aU: // PicoDrive
    return initMasterSystem();
  }

  return false;
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

bool Memory::initNes()
{
  static const Block blocks[] =
  {
    {RETRO_MEMORY_SYSTEM_RAM, 0x0000, "Work RAM"},
    {RETRO_MEMORY_SAVE_RAM,   0x6000, "Save RAM"},
    {0,                       0x0000, NULL}
  };

  _platform = Platform::kNes;

  bool ok = initWidthMmap(blocks);
  ok = ok || initWidthMdata(blocks);

  return ok;
}

bool Memory::initSnes()
{
  static const Block blocks[] =
  {
    {RETRO_MEMORY_SYSTEM_RAM, 0x000000, "Work RAM"},
    {RETRO_MEMORY_SAVE_RAM,   0x700000, "Save RAM"},
    {0,                       0x000000, NULL}
  };

  _platform = Platform::kSnes;

  bool ok = initWidthMmap(blocks);
  ok = ok || initWidthMdata(blocks);

  return ok;
}

bool Memory::initMasterSystem()
{
  static const Block blocks[] =
  {
    //{RETRO_MEMORY_SYSTEM_RAM, 0xc000, "Work RAM"},
    {RETRO_MEMORY_SYSTEM_RAM, 0x0000, "Work RAM"},
    {0,                       0x000000, NULL}
  };

  _platform = Platform::kMasterSystem;

  bool ok = initWidthMmap(blocks);
  ok = ok || initWidthMdata(blocks);

  for (auto it = _regions.begin(); it != _regions.end(); ++it)
  {
    if (it->_baseAddr == 0xc000)
    {
      // Remove mirror from system RAM
      //it->_size = 8192;
      break;
    }
  }

  return ok;
}
