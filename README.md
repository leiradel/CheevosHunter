# Cheevos Hunter

This was my [libretro](https://www.libretro.com/index.php/api/) frontend with integrated tooling to hunt for achievements, just like the tooling found in the [Retro Achievements](http://retroachievements.org/) official emulators.

![Cheevos Hunter](https://raw.githubusercontent.com/leiradel/CheevosHunter/master/ch.png)
![Cheevos Hunter](https://raw.githubusercontent.com/leiradel/CheevosHunter/master/ch2.png)

I've abandoned this project in favor of integrating the tooling in [RetroArch](https://github.com/fr500/RetroArch/tree/hunter). However, this code base has some interesting things worth sharing:

* `src/libretro/BareCore.[h|cpp]`: A C++ class with methods that map 1:1 to the libretro API implemented in libretro cores.
* `src/libretro/Core.[h|cpp]`: A wrap around BareCore, providing higher level functionality such as initializing a core, executing one emulated frame, and destroying the core. It supports multiple concurrent running cores, and uses composition to implement the platform specific code to display video frames and playing audio frames, reading input etc. Ready to use components that work with SDL2 can be found in `src/ImguiLibretro.[h|cpp]`.
* `src/libretro/Components.h`: The components that must be implemented to use with a Core.

A notable missing feature of this frontend is support for `RETRO_ENVIRONMENT_SET_HW_RENDER`. PRs are welcome.