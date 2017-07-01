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

void drawCoreInfo(libretro::Core* core)
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
    const struct retro_system_info* info = core->getSystemInfo();

    table(
      2,
      "library_name",     's', info->library_name,
      "library_version",  's', info->library_version,
      "valid_extensions", 's', info->valid_extensions,
      "need_fullpath",    'b', info->need_fullpath,
      "block_extract",    'b', info->block_extract,
      NULL
    );
  }

  if (ImGui::CollapsingHeader("retro_system_av_info"))
  {
    const struct retro_system_av_info* av_info = core->getSystemAVInfo();

    table(
      2,
      "base_width",   'u', av_info->geometry.base_width,
      "base_height",  'u', av_info->geometry.base_height,
      "max_width",    'u', av_info->geometry.max_width,
      "max_height",   'u', av_info->geometry.max_height,
      "aspect_ratio", 'f', av_info->geometry.aspect_ratio,
      "fps",          'f', av_info->timing.fps,
      "sample_rate",  'f', av_info->timing.sample_rate,
      NULL
    );
  }

  if (ImGui::CollapsingHeader("retro_input_descriptor"))
  {
    unsigned count;
    const struct retro_input_descriptor* desc = core->getInputDescriptors(&count);
    const struct retro_input_descriptor* end = desc + count;

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

    max.y = ImGui::GetItemsLineHeightWithSpacing() * (count < 16 ? count : 16);

    ImGui::BeginChild("retro_input_descriptor", max);
    ImGui::Columns(5, NULL, true);

    for (; desc < end; desc++)
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
      ImGui::Text("%u", desc->port);
      ImGui::NextColumn();
      ImGui::Text("(%u) %s", desc->device, device_names[desc->device]);
      ImGui::NextColumn();
      ImGui::Text("%u", desc->index);
      ImGui::NextColumn();
      ImGui::Text("(%2u) %s", desc->id, button_names[desc->id]);
      ImGui::NextColumn();
      ImGui::Text("%s", desc->description);
      ImGui::NextColumn();
    }

    ImGui::Columns(1);
    ImGui::Separator();
    ImGui::EndChild();
  }

  if (ImGui::CollapsingHeader("retro_controller_info"))
  {
    unsigned count;
    const struct retro_controller_info* info = core->getControllerInfo(&count);
    const struct retro_controller_info* end = info + count;

    ImGui::Columns(3, NULL, true);
    ImGui::Separator();
    ImGui::Text("port");
    ImGui::NextColumn();
    ImGui::Text("desc");
    ImGui::NextColumn();
    ImGui::Text("id");
    ImGui::NextColumn();

    for (count = 0; info < end; count++, info++)
    {
      const struct retro_controller_description* type = info->types;
      const struct retro_controller_description* end = type + info->num_types;

      for (; type < end; type++)
      {
        static const char* device_names[] =
        {
          "None", "Joypad", "Mouse", "Keyboard", "Lightgun", "Analog", "Pointer"
        };

        unsigned device = type->id & RETRO_DEVICE_MASK;

        ImGui::Separator();
        ImGui::Text("%u", count);
        ImGui::NextColumn();
        ImGui::Text("%s", type->desc);
        ImGui::NextColumn();
        ImGui::Text("(0x%04X) %s", type->id, device_names[device]);
        ImGui::NextColumn();
      }
    }

    ImGui::Columns(1);
    ImGui::Separator();
  }

  if (ImGui::CollapsingHeader("retro_variable"))
  {
    unsigned count;
    const struct retro_variable* var = core->getVariables(&count);
    const struct retro_variable* end = var + count;

    ImGui::Columns(2, NULL, true);
    ImGui::Separator();
    ImGui::Text("key");
    ImGui::NextColumn();
    ImGui::Text("value");
    ImGui::NextColumn();

    for (; var < end; var++)
    {
      ImGui::Separator();
      ImGui::Text("%s", var->key);
      ImGui::NextColumn();
      ImGui::Text("%s", var->value);
      ImGui::NextColumn();
    }

    ImGui::Columns(1);
    ImGui::Separator();
  }

  if (ImGui::CollapsingHeader("retro_subsystem_info"))
  {
    unsigned count;
    const struct retro_subsystem_info* info = core->getSubsystemInfo(&count);
    const struct retro_subsystem_info* end = info + count;

    for (; info < end; info++)
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
      ImGui::Text("%s", info->desc);
      ImGui::NextColumn();
      ImGui::Text("%s", info->ident);
      ImGui::NextColumn();
      ImGui::Text("%u", info->id);
      ImGui::NextColumn();

      ImGui::Columns(1);
      ImGui::Separator();

      const struct retro_subsystem_rom_info* rom = info->roms;
      const struct retro_subsystem_rom_info* end = rom + info->num_roms;

      for (unsigned i = 0; rom < end; i++, rom++)
      {
        ImGui::Indent();

        char title[64];
        snprintf(title, sizeof(title), "retro_subsystem_rom_info[%u]", i);

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
          ImGui::Text("%s", rom->desc);
          ImGui::NextColumn();
          ImGui::Text("%s", rom->valid_extensions);
          ImGui::NextColumn();
          ImGui::Text("%s", rom->need_fullpath ? "true" : "false");
          ImGui::NextColumn();
          ImGui::Text("%s", rom->block_extract ? "true" : "false");
          ImGui::NextColumn();
          ImGui::Text("%s", rom->required ? "true" : "false");
          ImGui::NextColumn();

          ImGui::Columns(1);
          ImGui::Separator();

          ImGui::Indent();

          const struct retro_subsystem_memory_info* mem = rom->memory;
          const struct retro_subsystem_memory_info* end = mem + rom->num_memory;

          for (unsigned j = 0; mem < end; j++, mem++)
          {
            char title[64];
            snprintf(title, sizeof(title), "retro_subsystem_memory_info[%u]", j);

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
            ImGui::Text("%s", mem->extension);
            ImGui::NextColumn();
            ImGui::Text("0x%08X", mem->type);
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
    const struct retro_memory_map* mmap = core->getMemoryMap();
    const struct retro_memory_descriptor* desc = mmap->descriptors;
    const struct retro_memory_descriptor* end = desc + mmap->num_descriptors;

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

    for (; desc < end; desc++)
    {
      char flags[7];

      flags[0] = 'M';

      if ((desc->flags & RETRO_MEMDESC_MINSIZE_8) == RETRO_MEMDESC_MINSIZE_8)
      {
        flags[1] = '8';
      }
      else if ((desc->flags & RETRO_MEMDESC_MINSIZE_4) == RETRO_MEMDESC_MINSIZE_4)
      {
        flags[1] = '4';
      }
      else if ((desc->flags & RETRO_MEMDESC_MINSIZE_2) == RETRO_MEMDESC_MINSIZE_2)
      {
        flags[1] = '2';
      }
      else
      {
        flags[1] = '1';
      }

      flags[2] = 'A';

      if ((desc->flags & RETRO_MEMDESC_ALIGN_8) == RETRO_MEMDESC_ALIGN_8)
      {
        flags[3] = '8';
      }
      else if ((desc->flags & RETRO_MEMDESC_ALIGN_4) == RETRO_MEMDESC_ALIGN_4)
      {
        flags[3] = '4';
      }
      else if ((desc->flags & RETRO_MEMDESC_ALIGN_2) == RETRO_MEMDESC_ALIGN_2)
      {
        flags[3] = '2';
      }
      else
      {
        flags[3] = '1';
      }

      flags[4] = desc->flags & RETRO_MEMDESC_BIGENDIAN ? 'B' : 'b';
      flags[5] = desc->flags & RETRO_MEMDESC_CONST ? 'C' : 'c';
      flags[6] = 0;

      ImGui::Separator();
      ImGui::Text("%s", flags);
      ImGui::NextColumn();
      ImGui::Text("%p", desc->ptr);
      ImGui::NextColumn();
      ImGui::Text("0x%08X", desc->offset);
      ImGui::NextColumn();
      ImGui::Text("0x%08X", desc->start);
      ImGui::NextColumn();
      ImGui::Text("0x%08X", desc->select);
      ImGui::NextColumn();
      ImGui::Text("0x%08X", desc->disconnect);
      ImGui::NextColumn();
      ImGui::Text("0x%08X", desc->len);
      ImGui::NextColumn();
      ImGui::Text("%s", desc->addrspace);
      ImGui::NextColumn();
    }

    ImGui::Columns(1);
    ImGui::Separator();
  }
}
