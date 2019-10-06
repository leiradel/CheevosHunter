#include "Memory.h"

#include "imguiext/imguial_fonts.h"
#include "imguiext/imguidock.h"
#include "imguiext/imgui_memory_editor.h"

#include <stdint.h>

bool Memory::init(libretro::CoreManager* core)
{
  _core = core;
  _selected = 0;
  return true;
}

void Memory::destroy()
{
  _map.clear();
}

void Memory::draw(bool running)
{
  if (ImGui::Begin(ICON_FA_MICROCHIP " Memory"))
  {
    if (running && _map.size() == 0)
    {
      addMemory(RETRO_MEMORY_SYSTEM_RAM, "System RAM ");
      addMemory(RETRO_MEMORY_SAVE_RAM,   "Save RAM   ");
      addMemory(RETRO_MEMORY_VIDEO_RAM,  "Video RAM  ");
      addMemory(RETRO_MEMORY_RTC,        "RTC RAM    ");

      std::vector<libretro::MemoryDescriptor> const& map = _core->getMemoryMap();

      for (auto const& desc : map)
      {
        if (desc.ptr == nullptr || desc.len == 0)
        {
          continue;
        }

        char name[64];
        int numWritten = snprintf(name, sizeof(name), "@%08X  ", (unsigned)desc.start);
        asMemorySize(name + numWritten, sizeof(name) - numWritten, desc.len);
        
        Region region;
        region.address = static_cast<uint32_t>(desc.start);
        region.data = static_cast<void*>(static_cast<uint8_t*>(desc.ptr) + desc.offset);
        region.size = desc.len;
        region.name = name;

        _map.emplace_back(std::move(region));
      }
    }

    struct Getter
    {
      static bool description(void* data, int idx, const char** out_text)
      {
        auto const map = (std::vector<Region>*)data;
        *out_text = (*map)[idx].name.c_str();

        return true;
      }
    };

    ImGui::Combo("Region", &_selected, Getter::description, (void*)&_map, _map.size());

    if (static_cast<size_t>(_selected) < _map.size())
    {
      static MemoryEditor editor;

      Region& region = _map[_selected];
      editor.Draw(region.name.c_str(), (unsigned char*)region.data, region.size);
    }
  }

  ImGui::End();
}

void Memory::reset()
{
  _map.clear();
  _selected = 0;
}

Snapshot Memory::click() const {
  if (static_cast<size_t>(_selected) < _map.size())
  {
    auto& region = _map[_selected];
    return Snapshot(region.address, region.data, region.size);
  }
  else
  {
    return Snapshot(0, nullptr, 0);
  }
}

void Memory::asMemorySize(char* str, size_t size, size_t numBytes)
{
  static char const* const units[] = {"bytes", "KiB", "MiB", "GiB", nullptr};

  for (int unit = 0; units[unit] != nullptr; unit++)
  {
    size_t rest = numBytes % 1024;

    if (rest != 0)
    {
      snprintf(str, size, "%4zu %s", numBytes, units[unit]);
      return;
    }

    numBytes /= 1024;
  }

  snprintf(str, size, "%zu GiB", numBytes / (1024 * 1024 * 1024));
}

void Memory::addMemory(unsigned id, char const* name)
{
  Region region;

  region.data = _core->getMemoryData(id);
  region.size = _core->getMemorySize(id);

  if (region.data == nullptr || region.size == 0)
  {
    return;
  }

  region.address = 0;

  char buffer[64];
  int numWritten = snprintf(buffer, sizeof(buffer), "%s", name);
  asMemorySize(buffer + numWritten, sizeof(buffer) - numWritten, region.size);

  region.name = buffer;
  _map.emplace_back(std::move(region));
}
