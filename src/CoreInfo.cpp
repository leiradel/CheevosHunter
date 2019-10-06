#include "CoreInfo.h"

#include <imgui.h>

#include <stdio.h>

static void table(int columns, ...)
{
  ImGui::Columns(columns, NULL, true);

  va_list args;
  va_start(args, columns);

  for (;;)
  {
    const char* name = va_arg(args, const char*);

    if (name == NULL)
    {
      break;
    }

    ImGui::Separator();
    ImGui::Text("%s", name);
    ImGui::NextColumn();

    for (int col = 1; col < columns; col++)
    {
      switch (va_arg(args, int))
      {
      case 's': ImGui::Text("%s", va_arg(args, const char*)); break;
      case 'd': ImGui::Text("%d", va_arg(args, int)); break;
      case 'u': ImGui::Text("%u", va_arg(args, unsigned)); break;
      case 'f': ImGui::Text("%f", va_arg(args, double)); break;
      case 'b': ImGui::Text("%s", va_arg(args, int) ? "true" : "false"); break;
      }

      ImGui::NextColumn();
    }
  }

  va_end(args);

  ImGui::Columns(1);
  ImGui::Separator();
}

void drawCoreInfo(libretro::CoreManager const* const core)
{
  if (ImGui::CollapsingHeader("Basic Information"))
  {
    static const char* pixel_formats[] =
    {
      "0RGB1555", "XRGB8888", "RGB565"
    };
    
    enum retro_pixel_format ndx = core->getPixelFormat();
    const char* pixel_format = "Unknown";

    if (ndx != RETRO_PIXEL_FORMAT_UNKNOWN)
    {
      pixel_format = pixel_formats[ndx];
    }

    table(
      2,
      "Api Version",           'u', core->getApiVersion(),
      "Region",                's', core->getRegion() == RETRO_REGION_NTSC ? "NTSC" : "PAL",
      "Rotation",              'u', core->getRotation() * 90,
      "Performance level",     'u', core->getPerformanceLevel(),
      "Pixel Format",          's', pixel_format,
      "Supports no Game",      'b', core->getSupportsNoGame(),
      "Supports Achievements", 'b', core->getSupportAchievements(),
      "Save RAM Size",         'u', core->getMemorySize(RETRO_MEMORY_SAVE_RAM),
      "RTC Size",              'u', core->getMemorySize(RETRO_MEMORY_RTC),
      "System RAM Size",       'u', core->getMemorySize(RETRO_MEMORY_SYSTEM_RAM),
      "Video RAM Size",        'u', core->getMemorySize(RETRO_MEMORY_VIDEO_RAM),
      NULL
    );
  }

  if (ImGui::CollapsingHeader("retro_system_info"))
  {
    libretro::SystemInfo const& info = core->getSystemInfo();

    table(
      2,
      "library_name",     's', info.libraryName,
      "library_version",  's', info.libraryVersion,
      "valid_extensions", 's', info.validExtensions,
      "need_fullpath",    'b', info.needFullpath,
      "block_extract",    'b', info.blockExtract,
      NULL
    );
  }

  if (ImGui::CollapsingHeader("retro_system_av_info"))
  {
    libretro::SystemAVInfo const& info = core->getSystemAVInfo();

    table(
      2,
      "base_width",   'u', info.geometry.baseWidth,
      "base_height",  'u', info.geometry.baseHeight,
      "max_width",    'u', info.geometry.maxWidth,
      "max_height",   'u', info.geometry.maxHeight,
      "aspect_ratio", 'f', info.geometry.aspectRatio,
      "fps",          'f', info.timing.fps,
      "sample_rate",  'f', info.timing.sampleRate,
      NULL
    );
  }

  if (ImGui::CollapsingHeader("retro_input_descriptor"))
  {
    std::vector<libretro::InputDescriptor> const& desc = core->getInputDescriptors();

    ImVec2 min = ImGui::GetWindowContentRegionMin();
    ImVec2 max = ImGui::GetWindowContentRegionMax();
    max.x -= min.x;

    max.y = ImGui::GetItemsLineHeightWithSpacing();

    ImGui::BeginChild("##empty", max);
    ImGui::Columns(5, NULL, true);
    ImGui::Separator();
    ImGui::Text("port");
    ImGui::NextColumn();
    ImGui::Text("device");
    ImGui::NextColumn();
    ImGui::Text("index");
    ImGui::NextColumn();
    ImGui::Text("id");
    ImGui::NextColumn();
    ImGui::Text("description");
    ImGui::NextColumn();
    ImGui::Columns( 1 );
    ImGui::Separator();
    ImGui::EndChild();

    max.y = ImGui::GetItemsLineHeightWithSpacing() * (desc.size() < 16 ? desc.size() : 16);

    ImGui::BeginChild("retro_input_descriptor", max);
    ImGui::Columns(5, NULL, true);

    for (auto const& element : desc)
    {
      static const char* device_names[] =
      {
        "None", "Joypad", "Mouse", "Keyboard", "Lightgun", "Analog", "Pointer"
      };

      static const char* button_names[] =
      {
        "B", "Y", "Select", "Start", "Up", "Down", "Left",
        "Right", "A", "X", "L", "R", "L2", "R2", "L3", "R3"
      };

      ImGui::Separator();
      ImGui::Text("%u", element.port);
      ImGui::NextColumn();
      ImGui::Text("(%u) %s", element.device, device_names[element.device]);
      ImGui::NextColumn();
      ImGui::Text("%u", element.index);
      ImGui::NextColumn();
      ImGui::Text("(%2u) %s", element.id, button_names[element.id]);
      ImGui::NextColumn();
      ImGui::Text("%s", element.description.c_str());
      ImGui::NextColumn();
    }

    ImGui::Columns(1);
    ImGui::Separator();
    ImGui::EndChild();
  }

  if (ImGui::CollapsingHeader("retro_controller_info"))
  {
    std::vector<libretro::ControllerInfo> const& info = core->getControllerInfo();

    ImGui::Columns(3, NULL, true);
    ImGui::Separator();
    ImGui::Text("port");
    ImGui::NextColumn();
    ImGui::Text("desc");
    ImGui::NextColumn();
    ImGui::Text("id");
    ImGui::NextColumn();

    unsigned port = 0;

    for (auto const& element : info)
    {
      for (auto const& element2 : element.types)
      {
        static const char* device_names[] =
        {
          "None", "Joypad", "Mouse", "Keyboard", "Lightgun", "Analog", "Pointer"
        };

        ImGui::Separator();
        ImGui::Text("%u", port);
        ImGui::NextColumn();
        ImGui::Text("%s", element2.desc.c_str());
        ImGui::NextColumn();
        ImGui::Text("(0x%04X) %s", element2.id, device_names[element2.id & RETRO_DEVICE_MASK]);
        ImGui::NextColumn();
      }

      port++;
    }

    ImGui::Columns(1);
    ImGui::Separator();
  }

  if (ImGui::CollapsingHeader("retro_variable"))
  {
    std::vector<libretro::Variable> const& vars = core->getVariables();

    ImGui::Columns(2, NULL, true);
    ImGui::Separator();
    ImGui::Text("key");
    ImGui::NextColumn();
    ImGui::Text("value");
    ImGui::NextColumn();

    for (auto const& element : vars)
    {
      ImGui::Separator();
      ImGui::Text("%s", element.key.c_str());
      ImGui::NextColumn();
      ImGui::Text("%s", element.value.c_str());
      ImGui::NextColumn();
    }

    ImGui::Columns(1);
    ImGui::Separator();
  }

  if (ImGui::CollapsingHeader("retro_subsystem_info"))
  {
    std::vector<libretro::SubsystemInfo> const& info = core->getSubsystemInfo();

    for (auto const& element : info)
    {
      ImGui::Columns(3, NULL, true);
      ImGui::Separator();
      ImGui::Text("desc");
      ImGui::NextColumn();
      ImGui::Text("ident");
      ImGui::NextColumn();
      ImGui::Text("id");
      ImGui::NextColumn();

      ImGui::Separator();
      ImGui::Text("%s", element.desc.c_str());
      ImGui::NextColumn();
      ImGui::Text("%s", element.ident.c_str());
      ImGui::NextColumn();
      ImGui::Text("%u", element.id);
      ImGui::NextColumn();

      ImGui::Columns(1);
      ImGui::Separator();

      unsigned index2 = 0;

      for (auto const& element2 : element.roms)
      {
        ImGui::Indent();

        char title[64];
        snprintf(title, sizeof(title), "retro_subsystem_rom_info[%u]", index2++);

        if (ImGui::CollapsingHeader(title))
        {
          ImGui::Columns(5, NULL, true);
          ImGui::Separator();
          ImGui::Text("desc" );
          ImGui::NextColumn();
          ImGui::Text("valid_extensions");
          ImGui::NextColumn();
          ImGui::Text("need_fullpath");
          ImGui::NextColumn();
          ImGui::Text("block_extract");
          ImGui::NextColumn();
          ImGui::Text("required");
          ImGui::NextColumn();

          ImGui::Separator();
          ImGui::Text("%s", element2.desc.c_str());
          ImGui::NextColumn();
          ImGui::Text("%s", element2.validExtensions.c_str());
          ImGui::NextColumn();
          ImGui::Text("%s", element2.needFullpath ? "true" : "false");
          ImGui::NextColumn();
          ImGui::Text("%s", element2.blockExtract ? "true" : "false");
          ImGui::NextColumn();
          ImGui::Text("%s", element2.required ? "true" : "false");
          ImGui::NextColumn();

          ImGui::Columns(1);
          ImGui::Separator();

          ImGui::Indent();

          unsigned index3 = 0;

          for (auto const& element3 : element2.memory)
          {
            char title[64];
            snprintf(title, sizeof(title), "retro_subsystem_memory_info[%u]", index3++);

            ImGui::Columns(3, NULL, true);
            ImGui::Separator();
            ImGui::Text("");
            ImGui::NextColumn();
            ImGui::Text("extension");
            ImGui::NextColumn();
            ImGui::Text("type");
            ImGui::NextColumn();

            ImGui::Separator();
            ImGui::Text("%s", title);
            ImGui::NextColumn();
            ImGui::Text("%s", element3.extension.c_str());
            ImGui::NextColumn();
            ImGui::Text("0x%08X", element3.type);
            ImGui::NextColumn();
          }

          ImGui::Unindent();
        }

        ImGui::Unindent();
      }
    }
  }

  if (ImGui::CollapsingHeader("retro_memory_map"))
  {
    std::vector<libretro::MemoryDescriptor> const& mmap = core->getMemoryMap();

    ImGui::Columns(8, NULL, true);
    ImGui::Separator();
    ImGui::Text("flags");
    ImGui::NextColumn();
    ImGui::Text("ptr");
    ImGui::NextColumn();
    ImGui::Text("offset");
    ImGui::NextColumn();
    ImGui::Text("start");
    ImGui::NextColumn();
    ImGui::Text("select");
    ImGui::NextColumn();
    ImGui::Text("disconnect");
    ImGui::NextColumn();
    ImGui::Text("len");
    ImGui::NextColumn();
    ImGui::Text("addrspace");
    ImGui::NextColumn();

    for (auto const& element : mmap)
    {
      char flags[7];

      flags[0] = 'M';

      if ((element.flags & RETRO_MEMDESC_MINSIZE_8) == RETRO_MEMDESC_MINSIZE_8)
      {
        flags[1] = '8';
      }
      else if ((element.flags & RETRO_MEMDESC_MINSIZE_4) == RETRO_MEMDESC_MINSIZE_4)
      {
        flags[1] = '4';
      }
      else if ((element.flags & RETRO_MEMDESC_MINSIZE_2) == RETRO_MEMDESC_MINSIZE_2)
      {
        flags[1] = '2';
      }
      else
      {
        flags[1] = '1';
      }

      flags[2] = 'A';

      if ((element.flags & RETRO_MEMDESC_ALIGN_8) == RETRO_MEMDESC_ALIGN_8)
      {
        flags[3] = '8';
      }
      else if ((element.flags & RETRO_MEMDESC_ALIGN_4) == RETRO_MEMDESC_ALIGN_4)
      {
        flags[3] = '4';
      }
      else if ((element.flags & RETRO_MEMDESC_ALIGN_2) == RETRO_MEMDESC_ALIGN_2)
      {
        flags[3] = '2';
      }
      else
      {
        flags[3] = '1';
      }

      flags[4] = element.flags & RETRO_MEMDESC_BIGENDIAN ? 'B' : 'b';
      flags[5] = element.flags & RETRO_MEMDESC_CONST ? 'C' : 'c';
      flags[6] = 0;

      ImGui::Separator();
      ImGui::Text("%s", flags);
      ImGui::NextColumn();
      ImGui::Text("%p", element.ptr);
      ImGui::NextColumn();
      ImGui::Text("0x%08X", (unsigned)element.offset);
      ImGui::NextColumn();
      ImGui::Text("0x%08X", (unsigned)element.start);
      ImGui::NextColumn();
      ImGui::Text("0x%08X", (unsigned)element.select);
      ImGui::NextColumn();
      ImGui::Text("0x%08X", (unsigned)element.disconnect);
      ImGui::NextColumn();
      ImGui::Text("0x%08X", (unsigned)element.len);
      ImGui::NextColumn();
      ImGui::Text("%s", element.addrspace.c_str());
      ImGui::NextColumn();
    }

    ImGui::Columns(1);
    ImGui::Separator();
  }
}
