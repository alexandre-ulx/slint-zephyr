# Slint + Zephyr RTOS Integration Example

A modern embedded UI framework ([Slint](https://slint.dev)) integrated with the [Zephyr RTOS](https://www.zephyrproject.org) using C++20. This project demonstrates a complete application combining real-time OS capabilities with a declarative UI layer.

## Prerequisites

Before starting, ensure you have installed:

- **Zephyr SDK** (v0.16.0 or later)
- **Python 3.8+** with pip
- **CMake 3.13.1+**
- **Rust** (with Cargo)
- **C++ toolchain** (compiler with C++20 support)

### Quick Setup

1. **Install West** (if not already installed):
   ```bash
   pip install west
   ```

2. **Initialize the workspace**:
   ```bash
   # initialize workspace
   west init -m https://github.com/alexandre-ulx/slint-zephyr --mr main slint-zephyr-workspace
   # update Zephyr modules
   cd slint-zephyr-workspace
   west update
   ```

## Supported Boards

This example supports the following development boards:

- **native_sim/native/64** - Linux x86_64 simulator (best for development)
- **stm32h750b_dk** - STM32H750 Discovery Kit with external flash
- **black_f407ve** - Black F407VE development board
- **mimxrt1170_evk** - NXP i.MX RT1170 evaluation kit

## Rust Target Configuration for ARM Microcontrollers

When compiling for ARM-based microcontrollers, Slint requires the correct Rust target architecture to be installed and configured. Each board requires a specific Rust compilation target:

### Identifying Your Microcontroller Architecture

1. **Find the microcontroller model** in your board's datasheet or board documentation
2. **Match the architecture** to the Rust cross-compilation target:
   - **STM32H750 (Cortex-M7)**: `thumbv7em-none-eabihf`
   - **STM32F407 (Cortex-M4)**: `thumbv7em-none-eabihf`
   - **NXP i.MX RT1170 (Cortex-M7)**: `thumbv7em-none-eabihf`
   - **Linux x86_64 (native_sim)**: `x86_64-unknown-linux-gnu`

### Installing Required Rust Targets

Install the necessary cross-compilation targets for your board:

```bash
# For ARM Cortex-M targets
rustup target add thumbv7em-none-eabihf

# For x86_64 (if not already present)
rustup target add x86_64-unknown-linux-gnu
```

Verify installation:
```bash
rustup target list | grep installed
```

### Automatic Target Detection

The build system (`app/CMakeLists.txt`) automatically detects your board and sets the correct Rust target. However, if you add a new board, ensure you:

1. Add a `elseif(BOARD MATCHES "your_board")` block in `CMakeLists.txt`
2. Set the correct `Rust_CARGO_TARGET` for that microcontroller's architecture
3. Rebuild with `west build -p always` to reconfigure

**Example** (for reference in `app/CMakeLists.txt`):
```cmake
elseif(BOARD STREQUAL "your_board")
    set(Rust_CARGO_TARGET "thumbv7em-none-eabihf")  # Change based on actual architecture
    set(BOARD_CONF_NAME "your_board")
```

## Project Structure

```
slint-zephyr/
├── app/                          # Application source
│   ├── src/                       # C++ implementation files
│   │   ├── main.cpp              # Entry point
│   │   ├── zephyr_api.cpp        # Zephyr OS integration
│   │   ├── display_backend.cpp   # Display driver interface
│   │   ├── input_bridge.cpp      # Input event handling
│   │   ├── slint_renderer_host.cpp # Slint rendering backend
│   │   └── platform_main_loop.cpp # Custom event loop
│   ├── ui/                        # Slint UI definitions
│   │   └── ui.slint              # UI layout & logic (declarative)
│   ├── boards/                    # Board-specific configurations
│   ├── CMakeLists.txt            # Build configuration
│   └── prj.conf                  # Zephyr kernel configuration
├── include/                       # Header files
├── modules/slint-ui/             # Slint framework (git submodule)
├── zephyr/                       # Zephyr RTOS (git submodule)
└── west.yml                      # West manifest for workspace
```

## Building the Application

> **Important**: Before building for an ARM-based board, verify you have installed the correct Rust target (see [Rust Target Configuration](#rust-target-configuration-for-arm-microcontrollers)).

### For Native Simulation (Linux)

```bash
cd slint-zephyr
west build -b native_sim/native/64 app -p always
```

Run the simulator:
```bash
./build/app/zephyr/zephyr.exe
```

### For STM32H750 Discovery Kit

```bash
west build -b stm32h750b_dk/stm32h750xx_m7 app -p always
```

Flash to device (requires pyocd or OpenOCD):
```bash
west flash
```

### For Black F407VE

```bash
west build -b black_f407ve app -p always
west flash
```

### Clean Build

```bash
west build -p always
```

The `-p always` flag forces a clean reconfigure.

## Key Configuration Files

### `app/prj.conf`
Main kernel configuration with enabled features:
- **C++ Support**: C++20 standard library with full libcpp
- **Display & Input**: Display subsystem and input device support
- **Shell**: Interactive shell for debugging
- **Flash Management**: XIP (Execute In Place) for external flash support
- **Floating Point**: Hardware FPU support (ARM boards)

### `app/CMakeLists.txt`
Build system configuration that:
- Detects target board and sets architecture-specific toolchain
- Integrates Slint as a subdirectory dependency
- Configures software renderer with SDF font support
- Links Slint and Zephyr libraries to the application

## Understanding the Code Architecture

### Component Flow

```
    ┌─────────────────────────┐
    │  Slint UI (ui.slint)    │  Declarative UI description
    └────────────┬────────────┘
                 │
    ┌────────────▼────────────────────────┐
    │  Platform Integration Layer         │
    │  (ZephyrPlatformMainLoop)           │
    ├──────────────┬──────────┬───────────┤
    │              │          │           │
    ▼              ▼          ▼           ▼
  Display       Renderer    Input     Event Loop
  Backend      (Software)   Bridge    (Timer-based)
```

### Main Components

- **main.cpp**: Application entry point, initializes Zephyr and Slint
- **zephyr_api.cpp**: Zephyr-specific initialization (devices, logging)
- **display_backend.cpp**: Display driver and framebuffer management
- **input_bridge.cpp**: Input event translation (touch, buttons, etc.)
- **slint_renderer_host.cpp**: Slint rendering loop integration
- **platform_main_loop.cpp**: Custom event loop for real-time operation

## Building Custom UI

Edit `app/ui/ui.slint` to create your user interface using Slint's declarative language:

```slint
Window {
    width: 320px;
    height: 240px;
    
    Text {
        text: "Hello from Slint!";
        color: #ffffff;
    }
}
```

Rebuild with `west build` - the build system automatically compiles `.slint` files to C++.

## Debugging

### Using Serial Console (Hardware)

```bash
minicom -D /dev/ttyUSB0 -b 115200
```

### Enable Debug Logging

Edit `app/debug.conf` or add to `prj.conf`:
```
CONFIG_LOG_LEVEL=4
CONFIG_APP_LOG_LEVEL=4
```

### Interactive Shell

Once built with `CONFIG_SHELL=y`, interact via:
```bash
zephyr> help
```

## Next Steps

1. **Customize the UI**: Modify `app/ui/ui.slint`
2. **Add business logic**: Extend `app/src/main.cpp` and Slint callbacks
3. **Integrate Zephyr features**: Use Zephyr APIs in C++ code
4. **Select target board**: Choose board via `-b` flag in `west build`
5. **Optimize for your hardware**: Adjust `prj.conf` for your needs

**Version**: 1.0 | **Updated**: February 2026 | **Status**: Ready for development
