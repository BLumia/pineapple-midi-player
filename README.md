## Summary

Simple SoundFont MIDI player.

Based on the following projects:

- [TinySoundFont and TinyMidiLoader](https://github.com/schellingb/TinySoundFont) (MIT License)
- [PortAudio](https://www.portaudio.com/) (MIT License)
- [Qt 6 (Widgets)](https://www.qt.io/) (LGPLv3 License)
- `stb_vorbis.c` from [stb](https://github.com/nothings/stb/) (Public Domain or MIT License)
- `dr_wav.h` from [dr_libs](https://github.com/mackron/dr_libs) (Public Domain or MIT License)
- `opl.h` from [dos-like](https://github.com/mattiasgustavsson/dos-like) (BSD 3-Clause License)

## Build it...

### ...manually

Ensure CMake, Qt 6 (at least 6.6) and PortAudio are available, then regular cmake building steps applies:

```shell
$ mkdir build && cd build
$ cmake ..
$ cmake --build . # or simply using `make` if you are using Makefile as the cmake generator.
```

### ...with Conan 2

<details>
<summary>This method is a little bit tedious so...</summary>

Conan can be used to build this project as well, but [PortAudio is still not available from Conan Center](https://github.com/conan-io/conan-center-index/issues/16335), so you will need to deal with the PortAudio dependency by yourself. You can either write a recipe/build the PortAudio Conan package by yourself, or use other method to ensure PortAudio can be found by CMake.

The following content can be saved to `conanfile.txt` for you to use:

```ini
[requires]
qt/6.6.2
portaudio/master

[generators]
CMakeDeps
CMakeToolchain

[layout]
cmake_layout

[options]
qt*:qttools=True
```

...and use the following commands to build it:

```shell
$ conan install . --build=missing
$ cmake . --preset conan-default -DCONAN2_STATIC_QT_BUG=ON
$ cmake --build --preset conan-release
```

The `CONAN2_STATIC_QT_BUG` option is required for Conan 2 build due to [this bug](https://github.com/conan-io/conan-center-index/issues/23045).

</details>

## License

Copyright &copy; 2024 [Gary Wang](https://github.com/BLumia/)

Available under Expat/MIT License
