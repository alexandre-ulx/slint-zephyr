#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(app, LOG_LEVEL_INF);

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/cache.h>
#include <cstdint>
#include "ui.h"
#include "zephyr_api.h"
#include <zephyr/autoconf.h>

int main(void)
{
    const struct device *display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
    if (!device_is_ready(display_dev)) {
        LOG_ERR("Display device not ready: %s", display_dev->name);
        return -1;
    }

#ifdef CONFIG_DISPLAY_TEST_PATTERN
    display_blanking_off(display_dev);
    display_capabilities cap{};
    display_get_capabilities(display_dev, &cap);

    LOG_INF("res=%ux%u fmt=%u", cap.x_resolution, cap.y_resolution, cap.current_pixel_format);

    void *fb = display_get_framebuffer(display_dev);
    if (!fb) {
        LOG_ERR("No framebuffer exposed by display driver");
        while (true) { k_sleep(K_SECONDS(1)); }
    }

    const size_t w = cap.x_resolution;
    const size_t h = cap.y_resolution;

    switch (cap.current_pixel_format) {
    case PIXEL_FORMAT_RGB_565: {
        auto *p = static_cast<uint16_t*>(fb);
        const size_t n = w * h;
        for (size_t i = 0; i < n; i++) p[i] = 0xF800;
        sys_cache_data_flush_range(fb, n * sizeof(uint16_t));
        break;
    }
    case PIXEL_FORMAT_ARGB_8888: {
        auto *p = static_cast<uint32_t*>(fb);
        const size_t n = w * h;
        for (size_t i = 0; i < n; i++) p[i] = 0xFFFF0000;
        sys_cache_data_flush_range(fb, n * sizeof(uint32_t));
        break;
    }
    case PIXEL_FORMAT_RGB_888: {
        auto *p = static_cast<uint8_t*>(fb);
        const size_t n = w * h;
        // RGB888: 3 bytes por pixel (R,G,B)
        for (size_t i = 0; i < n; i++) {
            p[i*3 + 0] = 0xFF; // R
            p[i*3 + 1] = 0x00; // G
            p[i*3 + 2] = 0x00; // B
        }
        sys_cache_data_flush_range(fb, n * 3);
        break;
    }
    default:
        LOG_ERR("Unsupported pixel format: %u", cap.current_pixel_format);
        break;
    }

    while (true) {
        k_sleep(K_SECONDS(1));
    }
#else
    LOG_INF("Starting Slint application");

    slint_zephyr_init(display_dev);
    LOG_INF("Platform initialized, creating window");

    display_set_contrast(display_dev, 30);
    display_set_brightness(display_dev, 50);

    auto main_window = MainWindow::create();
    LOG_INF("Window created, calling run()");

    main_window->run();
    
    LOG_INF("run() returned (should not reach here)");

    while (true) {
        k_sleep(K_SECONDS(1));
    }

#endif // CONFIG_DISPLAY_TEST_PATTERN
}