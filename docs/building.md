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
- On Windows, install the Vulkan SDK via a package manager such as scoop (`scoop install vulkan`) so the PATH entries are handled automatically.
- On Linux, Vulkan development packages should be available through your package manager.

## Setting up the CMake build system
You need the CMake build system and a C++ compiler to build this project.
### MacOS and Linux
You can get a compiler and CMake from your package manager. Any compiler should work.
### Windows
Install the required toolchain before configuring CMake:
1. Install **Visual Studio Build Tools 2022** with the C++ build tools using the Visual Studio Installer.
2. Install **Rust** (and Cargo) via [rustup](https://rustup.rs/).
3. Install the **Vulkan SDK** using a Windows package manager such as [scoop](https://scoop.sh/) (`scoop install vulkan`) so the paths are configured automatically.

After installing the tools, either launch CMake from the "Developer Command Prompt for VS..." (which already exposes the Visual Studio toolchain) or add the required executables such as `cmake` to your `PATH`. You can locate the Visual Studio CMake binaries by running `where cmake` inside that developer prompt; for example, CMake might live at `C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin`.

> A full system restart after installing the toolchain helps avoid lingering `PATH` issues before your first build.

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
