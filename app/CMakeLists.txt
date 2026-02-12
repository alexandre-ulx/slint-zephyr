cmake_minimum_required(VERSION 3.13.1)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(app LANGUAGES CXX)

if(NOT BOARD)
    message(FATAL_ERROR "No BOARD option specified")
endif()

# check is rust is available
find_program(RUSTC_EXECUTABLE rustc)
if(NOT RUSTC_EXECUTABLE)
    message(FATAL_ERROR "Rust compiler not found. Please install Rust and make sure it's in your PATH.")
endif()

# check board and set relevant variables
if(BOARD STREQUAL "native_sim")
    set(Rust_CARGO_TARGET "x86_64-unknown-linux-gnu")
    set(SLINT_LIBRARY_CARGO_FLAGS "-Zbuild-std=core,alloc")
    set(BOARD_CONF_NAME "native_sim_64")
elseif(BOARD STREQUAL "mimxrt1170_evk@B/mimxrt1176/cm7")
    set(Rust_CARGO_TARGET "thumbv7em-none-eabihf")
    set(BOARD_CONF_NAME "mimxrt1170_evk_mimxrt1176_cm7")
elseif(BOARD MATCHES "stm32h750b_dk*")
    set(Rust_CARGO_TARGET "thumbv7em-none-eabihf")
    set(BOARD_CONF_NAME "stm32h750b_dk")
elseif(BOARD MATCHES "black_f407ve")
    set(Rust_CARGO_TARGET "thumbv7em-none-eabihf")
    set(SLINT_LIBRARY_CARGO_FLAGS "")
    set(BOARD_CONF_NAME "black_f407ve")
else()
    message(FATAL_ERROR "Unsupported BOARD option specified: ${BOARD}")
endif()

set(SLINT_FEATURE_FREESTANDING ON CACHE BOOL "" FORCE)
set(SLINT_FEATURE_RENDERER_SOFTWARE ON CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF)  
set(DEFAULT_SLINT_EMBED_RESOURCES "embed-for-software-renderer-with-sdf" CACHE STRING "")

set(Rust_CARGO_TARGET_LINK_NATIVE_LIBS "" CACHE INTERNAL "" FORCE)
set(SLINT_FEATURE_SDF_FONTS ON CACHE BOOL "" FORCE)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../modules/slint-ui/ slint_build)

target_sources(app
    PRIVATE
    src/zephyr_api.cpp
    src/main.cpp
    src/display_backend.cpp
    src/input_bridge.cpp
    src/platform_main_loop.cpp
    src/slint_renderer_host.cpp
    src/slint_shell.cpp
)

target_include_directories(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include)
target_link_libraries(app PRIVATE Slint::Slint)

slint_target_sources(app ui/ui.slint)
