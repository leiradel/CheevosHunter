#pragma once

#include "libretro/Components.h"

#include <unordered_map>
#include <vector>
#include <string>

#include <SDL.h>
#include <SDL_opengl.h>

class Input: public libretro::InputComponent
{
public:
  bool init(libretro::LoggerComponent* logger);
  void destroy();
  void reset();
  void draw();

  void addController(int which);
  void processEvent(const SDL_Event* event);

  virtual void setInputDescriptors(std::vector<libretro::InputDescriptor> const& descs) override;

  virtual void     setControllerInfo(std::vector<libretro::ControllerInfo> const& infos) override;
  virtual bool     ctrlUpdated() override;
  virtual unsigned getController(unsigned port) override;

  virtual void    poll() override;
  virtual int16_t read(unsigned port, unsigned device, unsigned index, unsigned id) override;

protected:
  struct Pad
  {
    SDL_JoystickID      id;
    SDL_GameController* controller;
    std::string         controllerName;
    SDL_Joystick*       joystick;
    std::string         joystickName;
    int                 lastDir[6];
    bool                state[16];
    float               sensitivity;

    int      port;
    unsigned devId;
  };

  typedef libretro::InputDescriptor Descriptor;
  typedef libretro::ControllerDescription ControllerType;

  struct Controller
  {
    std::vector<ControllerType> types;
  };

  void drawPad(unsigned button);
  void addController(const SDL_Event* event);
  void removeController(const SDL_Event* event);
  void controllerButton(const SDL_Event* event);
  void controllerAxis(const SDL_Event* event);
  void keyboard(const SDL_Event* event);

  libretro::LoggerComponent* _logger;

  GLuint _texture;
  bool   _updated;

  std::unordered_map<SDL_JoystickID, Pad> _pads;
  std::vector<Descriptor>     _descriptors;
  std::vector<Controller>     _controllers;
  uint64_t                    _ports;
  std::vector<ControllerType> _ids[64];
};
