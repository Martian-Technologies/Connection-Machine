# Building
## Introduction
Connection Machine compiles on MacOS, Windows, and Linux. There are a few things you have to do in order to get it up and running:
1. Download the code
2. Install the dependencies (Vulkan, wasmtime)
3. Set up the CMake build system
Things will generally run smoothly on MacOS and Linux, there are a few more steps on Windows.

## Getting the source
You can start by git cloning the repository. A git client or the command line should work.

## System dependencies
This project requires [Vulkan SDK](https://www.lunarg.com/vulkan-sdk/) and [Wasmtime](https://github.com/bytecodealliance/wasmtime) to build.
- On MacOS, it is recommended to install `vulkan-tools`, `vulkan-validationlayers`, and `shaderc` using [brew](https://brew.sh/).
- On Windows, make sure to add the Vulkan SDK headers and binaries to your PATH.
- On Linux, Vulkan development packages should be available through your package manager.

## Setting up the CMake build system
You need the CMake build system and a C++ compiler to build this project.
### MacOS and Linux
You can get a compiler and CMake from your package manager. Any compiler should work.
### Windows
You can install CMake using the [CMake Binary Installer](https://cmake.org/download/).
You also need the MSVC compiler and its CMake build tools. You can download the installer from [Microsoft](https://visualstudio.microsoft.com/downloads/)
> In the Visual Studio installer, you probably want to do a custom installation, just make sure that you have C++ and CMake tools.
 
## Using CMake
Even if you are going to have your IDE manage CMake, it's a good idea to try running it from the terminal first.
> On Windows, Microsoft's build system will not be set as the CMake compiler by default. Either add MSBuild to your system variables, or use the "Developer Command Prompt for VS ..." application (which already has the variable set up) instead of your regular terminal. 

1. Configure - `cmake --preset debug`
2. Then Build - `cmake --build --preset debug`
3. Run the executable that was generated somewhere in the `build` directory. On Windows this is probably in the `Debug` subdirectory.

You can also build for release with `release` preset
> Works for MacOS, Windows (MSVC), and Linux

## Notes
If your error highlighting or IDE integration is showing red, make sure you have already compiled the project and the compile_commands.json in the build folder is being recognized (default for most lsp)
