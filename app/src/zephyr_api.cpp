#include "zephyr_api.h"
#include "platform_main_loop.h"
#include <slint-platform.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/display.h>

LOG_MODULE_REGISTER(zephyr_api_slint, LOG_LEVEL_DBG);

/**
 * @brief Initialize Slint platform with Zephyr
 * 
 * Creates and configures the complete Slint platform for Zephyr:
 * - DisplayBackendZephyr: abstracts display driver
 * - SlintRendererHost: manages rendering and buffering
 * - InputBridgeZephyr: lightweight input event handling
 * - ZephyrPlatformMainLoop: central event loop with proper wake management
 * 
 * This implements all phases from re-write-slint-api.md:
 * - Step 0: Metrics collection
 * - Step 1: Clean component architecture
 * - Step 2: Smart framebuffer strategy selection
 * - Step 3: Performance optimizations (pixel format conversion, rect coalescing)
 * - Step 4: Thread-safe input integration
 * - Step 5: Intelligent event loop with wake sources
 */
void slint_zephyr_init(const struct device *display)
{
    if (!display) {
        LOG_ERR("Display device is NULL");
        return;
    }

    // Turn off display blanking to show content
    display_blanking_off(display);

    // Create and configure the main platform with all components
    auto platform = std::make_unique<ZephyrPlatformMainLoop>(display);

    // Set as active Slint platform
    slint::platform::set_platform(std::move(platform));

    LOG_INF("Slint Zephyr platform initialized successfully");
}