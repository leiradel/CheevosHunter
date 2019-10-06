#include "Video.h"

#include <imgui.h>

bool Video::init(libretro::LoggerComponent* logger)
{
  _logger = logger;
  _texture = 0;
  _opened = true;
  _width = _height = 0;
  return true;
}

void Video::destroy()
{
  if (_texture != 0)
  {
    glDeleteTextures(1, &_texture);
  }
}

void Video::reset()
{
}

void Video::draw()
{
  if (_texture != 0)
  {
    ImVec2 min = ImGui::GetWindowContentRegionMin();
    ImVec2 max = ImGui::GetWindowContentRegionMax();

    float height = max.y - min.y;
    float width = height * _aspect;

    if (width > max.x - min.x)
    {
      width = max.x - min.x;
      height = width / _aspect;
    }

    ImVec2 size = ImVec2(width, height);
    ImVec2 uv0 = ImVec2(0.0f, 0.0f);

    ImVec2 uv1 = ImVec2(
      (float)_width / _textureWidth,
      (float)_height / _textureHeight
    );

    ImGui::Image((ImTextureID)(uintptr_t)_texture, size, uv0, uv1);
  }
}

bool Video::setGeometry(unsigned width, unsigned height, float aspect, enum retro_pixel_format pixelFormat)
{
  if (_texture != 0)
  {
    glDeleteTextures(1, &_texture);
  }

  GLint previous_texture;
  glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous_texture);
  
  glGenTextures(1, &_texture);
  glBindTexture(GL_TEXTURE_2D, _texture);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  
  switch (pixelFormat)
  {
  case RETRO_PIXEL_FORMAT_XRGB8888:
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    break;
    
  case RETRO_PIXEL_FORMAT_RGB565:
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, NULL);
    break;
    
  case RETRO_PIXEL_FORMAT_0RGB1555:
  default:
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_SHORT_1_5_5_5_REV, NULL);
    break;
  }

  glBindTexture(GL_TEXTURE_2D, previous_texture);
  _textureWidth = width;
  _textureHeight = height;
  _pixelFormat = pixelFormat;
  _aspect = aspect;

  _logger->printf(RETRO_LOG_DEBUG, "Geometry set to %u x %u (1:%f)", width, height, aspect);

  return true;
}

void Video::refresh(const void* data, unsigned width, unsigned height, size_t pitch)
{
  if (data != NULL && data != RETRO_HW_FRAME_BUFFER_VALID)
  {
    GLint previous_texture;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous_texture);
    
    glBindTexture(GL_TEXTURE_2D, _texture);
    uint8_t* p = (uint8_t*)data;

    GLenum type;
    
    switch (_pixelFormat)
    {
    case RETRO_PIXEL_FORMAT_XRGB8888:
      {
        uint32_t* q = (uint32_t*)alloca(width * 4);

        if (q != NULL)
        {
          for (unsigned y = 0; y < height; y++)
          {
            uint32_t* r = q;
            uint32_t* s = (uint32_t*)p;

            for (unsigned x = 0; x < width; x++)
            {
              uint32_t color = *s++;
              uint32_t red   = (color >> 16) & 255;
              uint32_t green = (color >> 8) & 255;
              uint32_t blue  = color & 255;
              *r++ = 0xff000000UL | blue << 16 | green << 8 | red;
            }

            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, y, width, 1, GL_RGBA, GL_UNSIGNED_BYTE, (void*)q);
            p += pitch;
          }
        }
      }
      
      goto end;
      
    case RETRO_PIXEL_FORMAT_RGB565:
      type = GL_UNSIGNED_SHORT_5_6_5;
      break;
      
    case RETRO_PIXEL_FORMAT_0RGB1555:
    default:
      type = GL_UNSIGNED_SHORT_1_5_5_5_REV;
      break;
    }

    for (unsigned y = 0; y < height; y++)
    {
      glTexSubImage2D(GL_TEXTURE_2D, 0, 0, y, width, 1, GL_RGB, type, (void*)p);
      p += pitch;
    }

  end:
    glBindTexture(GL_TEXTURE_2D, previous_texture);

    if (width != _width || height != _height)
    {
      _width = width;
      _height = height;

      _logger->printf(RETRO_LOG_DEBUG, "Video refreshed with geometry %u x %u", width, height);
    }
  }
}

uintptr_t Video::getCurrentFramebuffer()
{
  return 0;
}

void Video::showMessage(std::string const& msg, unsigned frames)
{
  _logger->printf(RETRO_LOG_INFO, "OSD message (%u): %s", frames, msg.c_str());
}
