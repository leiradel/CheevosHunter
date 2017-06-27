#include "Memory.h"

#include "imguiext/imguial_fonts.h"

#include <stdint.h>

bool Memory::init(libretro::Core* core, Platform platform)
{
  _core = core;
  _platform = platform;
  _opened = true;

  switch (platform)
  {
  case Platform::kNES:  return initNES();
  case Platform::kSNES: return initSNES();
  case Platform::kSMS:  return initSMS();
  default:              return false;
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

bool Memory::initNES()
{
  static const Block blocks[] =
  {
    {RETRO_MEMORY_SYSTEM_RAM, 0x0000, "Work RAM"},
    {RETRO_MEMORY_SAVE_RAM,   0x6000, "Save RAM"},
    {0,                       0x0000, NULL}
  };

  bool ok = initWidthMmap(blocks);
  ok = ok || initWidthMdata(blocks);

  return ok;
}

bool Memory::initSNES()
{
  static const Block blocks[] =
  {
    {RETRO_MEMORY_SYSTEM_RAM, 0x000000, "Work RAM"},
    {RETRO_MEMORY_SAVE_RAM,   0x700000, "Save RAM"},
    {0,                       0x000000, NULL}
  };

  bool ok = initWidthMmap(blocks);
  ok = ok || initWidthMdata(blocks);

  return ok;
}

bool Memory::initSMS()
{
  static const Block blocks[] =
  {
    {RETRO_MEMORY_SYSTEM_RAM, 0xc000, "Work RAM"},
    {0,                       0x0000, NULL}
  };

  bool ok = initWidthMmap(blocks);
  ok = ok || initWidthMdata(blocks);

  for (auto it = _regions.begin(); it != _regions.end(); ++it)
  {
    if (it->_baseAddr == 0xc000)
    {
      // Adjust Work RAM size because of the Genesis core
      it->_size = 8192;
      break;
    }
  }

  return ok;
}
