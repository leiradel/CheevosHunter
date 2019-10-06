#include "Input.h"

#include <imgui.h>

static const uint32_t s_controller[] =
{
  #include "controller.inl"
};

#define KEYBOARD_ID -1

bool Input::init(libretro::LoggerComponent* logger)
{
  _logger = logger;
  reset();

  GLint previous_texture;
  glGetIntegerv(GL_TEXTURE_BINDING_2D, &previous_texture);
  
  glGenTextures(1, &_texture);
  glBindTexture(GL_TEXTURE_2D, _texture);

  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 768, 192, 0, GL_RGBA, GL_UNSIGNED_BYTE, s_controller);

  glBindTexture(GL_TEXTURE_2D, previous_texture);

  // Add the keyboard controller
  Pad keyb;

  keyb.id = KEYBOARD_ID;
  keyb.controller = NULL;
  keyb.controllerName = "Keyboard";
  keyb.joystick = NULL;
  keyb.joystickName = "Keyboard";
  keyb.lastDir[0] = keyb.lastDir[1] = keyb.lastDir[2] =
  keyb.lastDir[3] = keyb.lastDir[4] = keyb.lastDir[5] = -1;
  memset(keyb.state, 0, sizeof(keyb.state));
  keyb.sensitivity = 0.5f;
  keyb.port = 0;
  keyb.devId = RETRO_DEVICE_NONE;

  _pads.insert(std::make_pair(keyb.id, keyb));
  _logger->printf(RETRO_LOG_INFO, "Controller %s (%s) added", keyb.controllerName.c_str(), keyb.joystickName.c_str());

  ControllerType ctrl;
  ctrl.desc = "None";
  ctrl.id = RETRO_DEVICE_NONE;

  for (unsigned i = 0; i < sizeof(_ids) / sizeof(_ids[0]); i++)
  {
    _ids[i].push_back(ctrl);
  }

  return true;
}

void Input::destroy()
{
  glDeleteTextures(1, &_texture);
}

void Input::reset()
{
  _updated = false;

  _descriptors.clear();
  _controllers.clear();

  _ports = 0;

  ControllerType ctrl;
  ctrl.desc = "None";
  ctrl.id = RETRO_DEVICE_NONE;

  for (unsigned i = 0; i < sizeof(_ids) / sizeof(_ids[0]); i++)
  {
    _ids[i].clear();
    _ids[i].push_back(ctrl);
  }

  for (auto pair : _pads)
  {
    Pad& pad = pair.second;

    pad.port = 0;
    pad.devId = RETRO_DEVICE_NONE;
  }
}

void Input::draw()
{
  unsigned count = 1;

  for (auto& pair : _pads)
  {
    Pad& pad = pair.second;

    char label[512];
    snprintf(label, sizeof(label), "%s (%u)", pad.controllerName.c_str(), count);

    if (ImGui::CollapsingHeader(label, ImGuiTreeNodeFlags_DefaultOpen))
    {
      char id[32];
      snprintf(id, sizeof(id), "%p", &pad);
      ImGui::Columns(2, id);

      {
        ImVec2 pos = ImGui::GetCursorPos();
        drawPad(17);

        for (unsigned button = 0; button < 16; button++)
        {
          if (pad.state[button])
          {
            ImGui::SetCursorPos(pos);
            drawPad(button);
          }
        }
      }

      ImGui::NextColumn();

      {
        char labels[1024];
        char* aux = labels;

        aux += snprintf(aux, sizeof(labels) - (aux - labels), "Disconnected") + 1;

        uint64_t bit = 1;

        for (unsigned i = 0; i < 64; i++, bit <<= 1)
        {
          if ((_ports & bit) != 0)
          {
            aux += snprintf(aux, sizeof(labels) - (aux - labels), "Connect to port %u", i + 1) + 1;
          }
        }

        *aux = 0;

        ImGui::PushItemWidth(-1.0f);

        char label[64];
        snprintf(label, sizeof(label), "##port%p", &pad);

        ImGui::Combo(label, &pad.port, labels);
        ImGui::PopItemWidth();
      }

      {
        char labels[512];
        unsigned ids[32];
        char* aux = labels;
        int count = 0;
        int selected = 0;

        aux += snprintf(aux, sizeof(labels) - (aux - labels), "None") + 1;
        ids[count++] = RETRO_DEVICE_NONE;

        if (_controllers.size() != 0)
        {
          if (pad.port > 0 && (size_t)pad.port <= _controllers.size())
          {
            Controller const& ctrl = _controllers[pad.port - 1];

            for (auto const& type : ctrl.types)
            {
              if ((type.id & RETRO_DEVICE_MASK) == RETRO_DEVICE_JOYPAD)
              {
                if (type.id == pad.devId)
                {
                  selected = count;
                }

                aux += snprintf(aux, sizeof(labels) - (aux - labels), "%s", type.desc.c_str()) + 1;
                ids[count++] = type.id;
              }
            }
          }
        }
        else
        {
          // No ports were specified, add the default RetroPad controller if the port is valid

          if (pad.port != 0)
          {
            aux += snprintf(aux, sizeof(labels) - (aux - labels), "RetroPad") + 1;

            if (pad.devId == RETRO_DEVICE_JOYPAD)
            {
              selected = 1;
            }
          }
        }

        *aux = 0;

        ImGui::PushItemWidth(-1.0f);

        char label[64];
        snprintf(label, sizeof(label), "##device%p", &pad);

        int old = selected;
        ImGui::Combo(label, &selected, labels);
        _updated = _updated || old != selected;

        if (_controllers.size() != 0)
        {
          pad.devId = ids[selected];
        }
        else
        {
          pad.devId = selected == 0 ? RETRO_DEVICE_NONE : RETRO_DEVICE_JOYPAD;
        }

        ImGui::PopItemWidth();
      }

      {
        char label[64];
        snprintf(label, sizeof(label), "##sensitivity%p", &pad);

        ImGui::PushItemWidth(-1.0f);
        ImGui::SliderFloat(label, &pad.sensitivity, 0.0f, 1.0f, "Sensitivity %.3f");
        ImGui::PopItemWidth();
      }

      ImGui::Columns(1);
    }
  }
}

void Input::drawPad(unsigned button)
{
  unsigned y = button / 6;
  unsigned x = button - y * 6;

  float xx = x * 128.0f;
  float yy = y * 64.0f;

  ImVec2 size = ImVec2(113.0f, 63.0f);
  ImVec2 uv0 = ImVec2(xx / 768.0f, yy / 192.0f);
  ImVec2 uv1 = ImVec2((xx + 128.0f) / 768.0f, (yy + 64.0f) / 192.0f);

  ImGui::Image((ImTextureID)(uintptr_t)_texture, size, uv0, uv1);
}

void Input::addController(int which)
{
  if (SDL_IsGameController(which))
  {
    Pad pad;

    pad.controller = SDL_GameControllerOpen(which);

    if (pad.controller == NULL)
    {
      _logger->printf(RETRO_LOG_ERROR, "Error opening the controller: %s", SDL_GetError());
      return;
    }

    pad.joystick = SDL_GameControllerGetJoystick(pad.controller);

    if (pad.joystick == NULL)
    {
      _logger->printf(RETRO_LOG_ERROR, "Error getting the joystick: %s", SDL_GetError());
      SDL_GameControllerClose(pad.controller);
      return;
    }

    pad.id = SDL_JoystickInstanceID(pad.joystick);

    if (_pads.find(pad.id) == _pads.end())
    {
      pad.controllerName = SDL_GameControllerName(pad.controller);
      pad.joystickName = SDL_JoystickName(pad.joystick);
      pad.lastDir[0] = pad.lastDir[1] = pad.lastDir[2] =
      pad.lastDir[3] = pad.lastDir[4] = pad.lastDir[5] = -1;
      pad.sensitivity = 0.5f;
      pad.port = 0;
      pad.devId = RETRO_DEVICE_NONE;
      memset(pad.state, 0, sizeof(pad.state));

      _pads.insert(std::make_pair(pad.id, pad));
      _logger->printf(RETRO_LOG_INFO, "Controller %s (%s) added", pad.controllerName.c_str(), pad.joystickName.c_str());
    }
    else
    {
      SDL_GameControllerClose(pad.controller);
    }
  }
}

void Input::processEvent(const SDL_Event* event)
{
  switch (event->type)
  {
  case SDL_CONTROLLERDEVICEADDED:
    addController(event);
    break;

  case SDL_CONTROLLERDEVICEREMOVED:
    removeController(event);
    break;

  case SDL_CONTROLLERBUTTONUP:
  case SDL_CONTROLLERBUTTONDOWN:
    controllerButton(event);
    break;

  case SDL_CONTROLLERAXISMOTION:
    controllerAxis(event);
    break;
  
  case SDL_KEYUP:
  case SDL_KEYDOWN:
    keyboard(event);
    break;
  }
}

void Input::setInputDescriptors(std::vector<libretro::InputDescriptor> const& descs)
{
  _descriptors.clear();
  _descriptors.reserve(descs.size());

  for (auto const& desc : descs)
  {
    _descriptors.emplace_back(desc);

    unsigned port = desc.port;

    if (port < sizeof(_ids) / sizeof(_ids[0]))
    {
      ControllerType ctrl;
      ctrl.desc = "RetroPad";
      ctrl.id = RETRO_DEVICE_JOYPAD;

      _ids[port].emplace_back(ctrl);
      _ports |= UINT64_C(1) << port;
    }
  }
}

void Input::setControllerInfo(std::vector<libretro::ControllerInfo> const& infos)
{
  _controllers.clear();
  _controllers.resize(infos.size());

  for (size_t i = 0; i < infos.size(); i++)
  {
    libretro::ControllerInfo const& source = infos[i];
    Controller& target = _controllers[i];

    target.types.clear();
    target.types.reserve(source.types.size());

    for (auto const& type : source.types)
    {
      target.types.emplace_back(type);

      if ((type.id & RETRO_DEVICE_MASK) == RETRO_DEVICE_JOYPAD)
      {
        unsigned port = i;

        if (port < sizeof(_ids) / sizeof(_ids[0]))
        {
          bool found = false;

          for (auto& element : _ids[port])
          {
            if (element.id == type.id)
            {
              // Overwrite the generic RetroPad description with the one from the controller info
              element.desc = type.desc;
              found = true;
              break;
            }
          }

          if (!found)
          {
            _ids[port].push_back(type);
          }

          _ports |= UINT64_C(1) << port;
        }
      }
    }
  }
}

bool Input::ctrlUpdated()
{
  bool updated = _updated;
  _updated = false;
  return updated;
}

unsigned Input::getController(unsigned port)
{
  port++;

  for (auto const& pair : _pads)
  {
    auto const& pad = pair.second;

    if (pad.port == (int)port)
    {
      return pad.devId;
    }
  }

  // The controller was removed
  return RETRO_DEVICE_NONE;
}

void Input::poll()
{
  // Events are polled in the main event loop, and arrive in this class via
  // the processEvent method
}

int16_t Input::read(unsigned port, unsigned device, unsigned index, unsigned id)
{
  (void)index;

  port++;

  for (auto const& pair : _pads)
  {
    Pad const& pad = pair.second;

    if (pad.port == (int)port && pad.devId == device)
    {
      return pad.state[id] ? 32767 : 0;
    }
  }

  return 0;
}

void Input::addController(const SDL_Event* event)
{
  addController(event->cdevice.which);
}

void Input::removeController(const SDL_Event* event)
{
  auto it = _pads.find(event->cdevice.which);

  if (it != _pads.end())
  {
    Pad const& pad = it->second;
    _logger->printf(RETRO_LOG_INFO, "Controller %s (%s) removed", pad.controllerName.c_str(), pad.joystickName.c_str());
    SDL_GameControllerClose(pad.controller);
    _pads.erase(it);

    // Flag a pending update so the core will receive an event for this removal
    _updated = true;
  }
}

void Input::controllerButton(const SDL_Event* event)
{
  auto it = _pads.find(event->cbutton.which);

  if (it != _pads.end())
  {
    Pad& pad = it->second;
    unsigned button;

    switch (event->cbutton.button)
    {
    case SDL_CONTROLLER_BUTTON_A:             button = RETRO_DEVICE_ID_JOYPAD_B; break;
    case SDL_CONTROLLER_BUTTON_B:             button = RETRO_DEVICE_ID_JOYPAD_A; break;
    case SDL_CONTROLLER_BUTTON_X:             button = RETRO_DEVICE_ID_JOYPAD_Y; break;
    case SDL_CONTROLLER_BUTTON_Y:             button = RETRO_DEVICE_ID_JOYPAD_X; break;
    case SDL_CONTROLLER_BUTTON_BACK:          button = RETRO_DEVICE_ID_JOYPAD_SELECT; break;
    case SDL_CONTROLLER_BUTTON_START:         button = RETRO_DEVICE_ID_JOYPAD_START; break;
    case SDL_CONTROLLER_BUTTON_LEFTSTICK:     button = RETRO_DEVICE_ID_JOYPAD_L3; break;
    case SDL_CONTROLLER_BUTTON_RIGHTSTICK:    button = RETRO_DEVICE_ID_JOYPAD_R3; break;
    case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:  button = RETRO_DEVICE_ID_JOYPAD_L; break;
    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: button = RETRO_DEVICE_ID_JOYPAD_R; break;
    case SDL_CONTROLLER_BUTTON_DPAD_UP:       button = RETRO_DEVICE_ID_JOYPAD_UP; break;
    case SDL_CONTROLLER_BUTTON_DPAD_DOWN:     button = RETRO_DEVICE_ID_JOYPAD_DOWN; break;
    case SDL_CONTROLLER_BUTTON_DPAD_LEFT:     button = RETRO_DEVICE_ID_JOYPAD_LEFT; break;
    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:    button = RETRO_DEVICE_ID_JOYPAD_RIGHT; break;
    case SDL_CONTROLLER_BUTTON_GUIDE:         // fallthrough
    default:                                  return;
    }

    pad.state[button] = event->cbutton.state == SDL_PRESSED;
  }
}

void Input::controllerAxis(const SDL_Event* event)
{
  auto it = _pads.find(event->caxis.which);

  if (it != _pads.end())
  {
    Pad& pad = it->second;
    int threshold = 32767 * pad.sensitivity;
    int positive, negative;
    int button;
    int *last_dir;

    switch (event->caxis.axis)
    {
    case SDL_CONTROLLER_AXIS_LEFTX:
    case SDL_CONTROLLER_AXIS_LEFTY:
    case SDL_CONTROLLER_AXIS_RIGHTX:
    case SDL_CONTROLLER_AXIS_RIGHTY:
      switch (event->caxis.axis)
      {
      case SDL_CONTROLLER_AXIS_LEFTX:
        positive = RETRO_DEVICE_ID_JOYPAD_RIGHT;
        negative = RETRO_DEVICE_ID_JOYPAD_LEFT;
        last_dir = pad.lastDir + 0;
        break;

      case SDL_CONTROLLER_AXIS_LEFTY:
        positive = RETRO_DEVICE_ID_JOYPAD_DOWN;
        negative = RETRO_DEVICE_ID_JOYPAD_UP;
        last_dir = pad.lastDir + 1;
        break;

      case SDL_CONTROLLER_AXIS_RIGHTX:
        positive = RETRO_DEVICE_ID_JOYPAD_RIGHT;
        negative = RETRO_DEVICE_ID_JOYPAD_LEFT;
        last_dir = pad.lastDir + 2;
        break;

      case SDL_CONTROLLER_AXIS_RIGHTY:
        positive = RETRO_DEVICE_ID_JOYPAD_DOWN;
        negative = RETRO_DEVICE_ID_JOYPAD_UP;
        last_dir = pad.lastDir + 3;
        break;
      }

      if (event->caxis.value < -threshold)
      {
        button = negative;
      }
      else if (event->caxis.value > threshold)
      {
        button = positive;
      }
      else
      {
        button = -1;
      }

      break;

    case SDL_CONTROLLER_AXIS_TRIGGERLEFT:
    case SDL_CONTROLLER_AXIS_TRIGGERRIGHT:
      if (event->caxis.axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT)
      {
        button = RETRO_DEVICE_ID_JOYPAD_L2;
        last_dir = pad.lastDir + 4;
      }
      else
      {
        button = RETRO_DEVICE_ID_JOYPAD_R2;
        last_dir = pad.lastDir + 5;
      }

      break;

    default:
      return;
    }

    if (*last_dir != -1)
    {
      pad.state[*last_dir] = false;
    }

    if (event->caxis.value < -threshold || event->caxis.value > threshold)
    {
      pad.state[button] = true;
    }

    *last_dir = button;
  }
}

void Input::keyboard(const SDL_Event* event)
{
  if (event->key.repeat)
  {
    return;
  }

  SDL_Event evt;
  evt.cbutton.which = KEYBOARD_ID;
  evt.cbutton.state = event->key.state;

  switch (event->key.keysym.sym)
  {
    case SDLK_s:         evt.cbutton.button = SDL_CONTROLLER_BUTTON_A; break;
    case SDLK_d:         evt.cbutton.button = SDL_CONTROLLER_BUTTON_B; break;
    case SDLK_a:         evt.cbutton.button = SDL_CONTROLLER_BUTTON_X; break;
    case SDLK_w:         evt.cbutton.button = SDL_CONTROLLER_BUTTON_Y; break;
    case SDLK_BACKSPACE: evt.cbutton.button = SDL_CONTROLLER_BUTTON_BACK; break;
    case SDLK_RETURN:    evt.cbutton.button = SDL_CONTROLLER_BUTTON_START; break;
    case SDLK_1:         evt.cbutton.button = SDL_CONTROLLER_BUTTON_LEFTSTICK; break;
    case SDLK_3:         evt.cbutton.button = SDL_CONTROLLER_BUTTON_RIGHTSTICK; break;
    case SDLK_q:         evt.cbutton.button = SDL_CONTROLLER_BUTTON_LEFTSHOULDER; break;
    case SDLK_e:         evt.cbutton.button = SDL_CONTROLLER_BUTTON_RIGHTSHOULDER; break;
    case SDLK_UP:        evt.cbutton.button = SDL_CONTROLLER_BUTTON_DPAD_UP; break;
    case SDLK_DOWN:      evt.cbutton.button = SDL_CONTROLLER_BUTTON_DPAD_DOWN; break;
    case SDLK_LEFT:      evt.cbutton.button = SDL_CONTROLLER_BUTTON_DPAD_LEFT; break;
    case SDLK_RIGHT:     evt.cbutton.button = SDL_CONTROLLER_BUTTON_DPAD_RIGHT; break;
    case SDLK_2:         evt.cbutton.button = SDL_CONTROLLER_BUTTON_GUIDE; break;
    default:                                  return;
  }

  controllerButton(&evt);
}
