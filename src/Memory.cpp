#include "Memory.h"

#include "imguiext/imguial_fonts.h"
#include "imguiext/imguidock.h"
#include "imguiext/imgui_memory_editor.h"

#include <stdint.h>

bool Memory::init(libretro::Core* core)
{
  _core = core;
  _region = 0;
  _platform = 0;
  return true;
}

void Memory::destroy()
{
  _regions.clear();
}

void Memory::draw(bool running)
{
  if (ImGui::BeginDock(ICON_FA_FILTER " Filters"))
    ImGui::Text("filters");
  ImGui::EndDock();

  if (ImGui::BeginDock(ICON_FA_MICROCHIP " Memory"))
  {
    static const char* platforms[] =
    {
      "None",
      "Nintendo Entertainment System",
      "Super Nintendo Entertainment System",
      "Sega Master System",
      "Sega Mega Drive"
    };
    
    int old = _platform;
    ImGui::Combo("Platform", &_platform, platforms, running ? sizeof(platforms) / sizeof(platforms[0]) : 1);

    if (old != _platform)
    {
      _regions.clear();
      _region = 0;

      switch (_platform)
      {
      case 1: // NES
        initNES();
        break;
      
      case 2: // SNES
        initSNES();
        break;
      
      case 3: // SMS
        initSMS();
        break;
      
      case 4: // MD
        initMD();
        break;
      }
    }

    struct Getter
    {
      static bool description(void* data, int idx, const char** out_text)
      {
        if (idx == 0)
        {
          *out_text = "None";
        }
        else
        {
          auto regions = (std::vector<Region>*)data;
          *out_text = (*regions)[idx - 1]._description.c_str();
        }

        return true;
      }
    };

    ImGui::Combo("Region", &_region, Getter::description, (void*)&_regions, _regions.size() + 1);

    if (_region != 0)
    {
      static MemoryEditor editor;

      Region* region = &_regions[_region - 1];
      editor.Draw(region->_description.c_str(), (unsigned char*)region->_contents, region->_size, region->_baseAddr);
    }
  }

  ImGui::EndDock();
}

void Memory::reset()
{
  _region = 0;
  _platform = 0;
  _regions.clear();
}

bool Memory::initWidthMmap(const Block* block)
{
  const struct retro_memory_map* mmap = _core->getMemoryMap();

  if (mmap == NULL || mmap->num_descriptors == 0)
  {
    return false;
  }

  for (int i = 0; block->_description != NULL; i++, block++)
  {
    const struct retro_memory_descriptor* desc = mmap->descriptors;
    bool found = false;

    for (unsigned j = 0; j < mmap->num_descriptors; j++, desc++)
    {
      if (desc->start == block->_start && (block->_size != 0 && desc->len >= block->_size))
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

    region._description = block->_description;
    region._description.append(" (");
    region._description.append(block->_identifier);
    region._description.append(")");

    region._contents = desc->ptr;
    region._size     = desc->len;
    region._baseAddr = desc->start;

    _regions.push_back(region);
  }

  return true;
}

bool Memory::initWidthMdata(const Block* block)
{
  for (int i = 0; block->_description != NULL; i++, block++)
  {
    void* contents = _core->getMemoryData(block->_memid);

    if (contents == NULL)
    {
      _regions.clear();
      return false;
    }

    size_t size = _core->getMemorySize(block->_memid);

    if (block->_size != 0 && size < block->_size)
    {
      _regions.clear();
      return false;
    }

    Region region;

    region._description = ICON_FA_MICROCHIP;
    region._description.append(" ");
    region._description.append(block->_description);
    region._description.append(" (");
    region._description.append(block->_identifier);
    region._description.append(")");

    region._contents = contents;
    region._size     = size;
    region._baseAddr = block->_start;

    _regions.push_back(region);
  }

  return true;
}

bool Memory::initNES()
{
  static const Block blocks[] =
  {
    {RETRO_MEMORY_SYSTEM_RAM, 0x0000, 2 * 1024, "wram", "Work RAM"},
    {RETRO_MEMORY_SAVE_RAM,   0x6000, 8 * 1024, "sram", "Save RAM"},
    {0}
  };

  bool ok = initWidthMmap(blocks);
  ok = ok || initWidthMdata(blocks);

  return ok;
}

bool Memory::initSNES()
{
  static const Block blocks[] =
  {
    {RETRO_MEMORY_SAVE_RAM,   0x700000,          0, "sram", "Save RAM"},
    {RETRO_MEMORY_SYSTEM_RAM, 0x7e0000, 128 * 1024, "wram", "Work RAM"},
    {0}
  };

  bool ok = initWidthMmap(blocks);
  ok = ok || initWidthMdata(blocks);

  return ok;
}

bool Memory::initSMS()
{
  static const Block blocks[] =
  {
    {RETRO_MEMORY_SYSTEM_RAM, 0xc000, 8 * 1024, "wram", "Work RAM"},
    {0}
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

bool Memory::initMD()
{
  static const Block blocks[] =
  {
    {RETRO_MEMORY_SYSTEM_RAM, 0xff0000, 64 * 1024, "wram", "Work RAM"},
    {0}
  };

  bool ok = initWidthMmap(blocks);
  ok = ok || initWidthMdata(blocks);

  return ok;
}
