## Summary

Simple SoundFont MIDI player.

Based on the following projects:

- [TinySoundFont and TinyMidiLoader](https://github.com/schellingb/TinySoundFont) (MIT License)
- [PortAudio](https://www.portaudio.com/) (MIT License)
- [Qt 6 (Widgets)](https://www.qt.io/) (LGPLv3 License)
- `stb_vorbis.c` from [stb](https://github.com/nothings/stb/) (Public Domain or MIT License)
- `dr_wav.h` from [dr_libs](https://github.com/mackron/dr_libs) (Public Domain or MIT License)
- `opl.h` from [dos-like](https://github.com/mattiasgustavsson/dos-like) (BSD 3-Clause License)
- *(optional)* [KIO](https://invent.kde.org/frameworks/kio) (LGPLv2+ License)

## Build it...

### ...manually

Ensure CMake, Qt 6 (at least 6.4) and PortAudio are available, then regular cmake building steps applies:

```shell
$ mkdir build && cd build
$ cmake ..
$ cmake --build . # or simply using `make` if you are using Makefile as the cmake generator.
```

### ...with Conan 2

<details>
<summary>This method is a little bit tedious so...</summary>

By default, `conan profile detect` will generate a profile with `compiler.cppstd=14`, you will need to change it to or use a profile with `compiler.cppstd=17` in order to build Qt.

[Conan Center doesn't have PortAudio package](https://github.com/conan-io/conan-center-index/issues/16335), so this project uses `FetchContent` to download and build it. You can manually build it and use the version that you manually built as well if preferred as long as `find_package` can find it.

The following content can be saved to `conanfile.txt` for you to use:

```ini
[requires]
qt/6.8.3

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

Copyright &copy; 2025 [Gary Wang](https://github.com/BLumia/)

Available under Expat/MIT License
