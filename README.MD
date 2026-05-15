[![MSBuild](https://github.com/Cynichill/Diva-Archipelago-Mod/actions/workflows/msbuild.yml/badge.svg)](https://github.com/Cynichill/Diva-Archipelago-Mod/actions/workflows/msbuild.yml)

Prebuilt Release and Debug versions can be found in [Actions](https://github.com/Cynichill/Diva-Archipelago-Mod/actions).
Builds happen on commit and automatically weekly.

### Building (local)

Archipelago Mod and APCpp build targets **need to be the same.** If you experience crashes, this is most likely why.
You may need to create a `x64-Release` build config for APCpp if it does not exist and you want to build the AP Mod in Release.
If there are linker errors fix them but the default paths should be fine.

1. Clone recursively with `git clone --recurse-submodules`
   - [APCpp](https://github.com/N00byKing/APCpp) is pinned as a submodule, and has pinned submodules of its own.
2. In MSVC:
   - File > Open > Folder (Ctrl+Alt+Shift+O)
   - Open the APCpp folder
   - Right-click `CMakeLists.txt` in Solution Explorer > Build
3. Copy/move/symlink `APCpp\out\build\x64-CONFIG\APCpp.dll` to the game's Archipelago Mod folder.
   - `CONFIG` being your build config.
   - This is dynamically linked when the mod loads. No need to edit `config.toml`.
4. Build Archipelago Mod as normal.
5. Copy/move/symlink the DLL to the game's Archipelago Mod folder.

### Building (GitHub action)

See the [workflow file.](.github/workflows/msbuild.yml)
