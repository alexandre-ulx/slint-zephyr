#include "slint_shell.h"
#include "slint_renderer_host.h"
#include "display_backend.h"
#include <zephyr/shell/shell.h>
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(zephyr_api_slint, LOG_LEVEL_INF);

// Forward declarations - these will be set during platform initialization
static SlintRendererHost *g_renderer_host = nullptr;
static DisplayBackendZephyr *g_display_backend = nullptr;

/**
 * @brief Register renderer and display backend for shell access
 * Called by platform initialization
 */
void slint_shell_register_renderer(SlintRendererHost *renderer)
{
    g_renderer_host = renderer;
}

void slint_shell_register_display(DisplayBackendZephyr *display)
{
    g_display_backend = display;
}

// Shell command: slint status
static int cmd_slint_status(const struct shell *sh, size_t argc, char **argv)
{
    if (!g_renderer_host) {
        shell_print(sh, "Slint platform not initialized");
        return -ENODEV;
    }

    const auto &metrics = g_renderer_host->metrics();

    shell_print(sh, "\n=== Slint Rendering Status ===");
    shell_print(sh, "Display: %ux%u pixels", metrics.display_width, metrics.display_height);
    shell_print(sh, "Frame #: %u", metrics.frame_count);

    if (metrics.frame_count > 0) {
        shell_print(sh, "\nTiming (frame):");
        shell_print(sh, "  Render: %lld ms (avg: %.1f ms)", metrics.slint_render_time_ms,
                    metrics.get_average_render_time());
        shell_print(sh, "  Display: %lld ms (avg: %.1f ms)", metrics.display_write_time_ms,
                    metrics.get_average_write_time());
        shell_print(sh, "  Total: %lld ms (min: %lld, max: %lld)", metrics.total_frame_time_ms,
                    metrics.min_frame_time_ms, metrics.max_frame_time_ms);
        shell_print(sh, "  FPS: %.1f", metrics.get_fps());

        shell_print(sh, "\nDirty regions (last frame):");
        shell_print(sh, "  Count: %u", metrics.num_dirty_rects);
        shell_print(sh, "  Pixels: %u", metrics.total_dirty_pixels);
        shell_print(sh, "  Max rect: %ux%u", metrics.max_dirty_rect_width,
                    metrics.max_dirty_rect_height);

        shell_print(sh, "\nMemory:");
        shell_print(sh, "  Buffer: %u bytes", metrics.buffer_size_bytes);
        shell_print(sh, "  Hardware FB: %s", metrics.has_hardware_framebuffer ? "yes" : "no");
        shell_print(sh, "  Using direct FB: %s",
                    metrics.using_framebuffer_direct ? "yes" : "no");

        shell_print(sh, "\nStrategy:");
        shell_print(sh, "  Partial updates: %s", metrics.partial_updates_supported ? "yes" : "no");
    } else {
        shell_print(sh, "No frames rendered yet");
    }

    return 0;
}

// Shell command: slint config
static int cmd_slint_config(const struct shell *sh, size_t argc, char **argv)
{
    if (!g_display_backend) {
        shell_print(sh, "Display backend not initialized");
        return -ENODEV;
    }

    const auto &caps = g_display_backend->get_capabilities();

    shell_print(sh, "\n=== Display Driver Configuration ===");
    shell_print(sh, "Resolution: %ux%u", caps.width, caps.height);

    shell_print(sh, "\nPixel Format: ");
    switch (caps.current_format) {
    case PIXEL_FORMAT_RGB_565:
        shell_print(sh, "  RGB565");
        break;
    case PIXEL_FORMAT_BGR_565:
        shell_print(sh, "  BGR565");
        break;
    case PIXEL_FORMAT_RGB_888:
        shell_print(sh, "  RGB888");
        break;
    case PIXEL_FORMAT_ARGB_8888:
        shell_print(sh, "  ARGB8888");
        break;
    default:
        shell_print(sh, "  Unknown (%d)", caps.current_format);
    }

    shell_print(sh, "\nCapabilities:");
    shell_print(sh, "  Framebuffer: %s", caps.has_framebuffer ? "yes" : "no");
    shell_print(sh, "  Partial updates: %s", caps.supports_partial_updates ? "yes" : "no");
    shell_print(sh, "  Double buffering: %s", caps.double_buffering ? "yes" : "no");

    shell_print(sh, "\nStrategy: ");
    switch (caps.strategy) {
    case DisplayBackendZephyr::FrameBufferStrategy::DirectFrameBuffer:
        shell_print(sh, "  Direct Framebuffer (2A)");
        break;
    case DisplayBackendZephyr::FrameBufferStrategy::FullBuffer:
        shell_print(sh, "  Full Buffer (2B)");
        break;
    case DisplayBackendZephyr::FrameBufferStrategy::PartialUpdates:
        shell_print(sh, "  Partial Updates (2C)");
        break;
    }

    return 0;
}

// Shell command: slint metrics
static int cmd_slint_metrics(const struct shell *sh, size_t argc, char **argv)
{
    if (argc < 2) {
        shell_print(sh, "Usage: slint metrics <reset|history>");
        return -EINVAL;
    }

    if (!g_renderer_host) {
        shell_print(sh, "Slint platform not initialized");
        return -ENODEV;
    }

    if (strcmp(argv[1], "reset") == 0) {
        auto &metrics = g_renderer_host->metrics();
        metrics.frame_count = 0;
        metrics.accumulated_render_time = 0;
        metrics.accumulated_write_time = 0;
        metrics.min_frame_time_ms = INT64_MAX;
        metrics.max_frame_time_ms = 0;
        shell_print(sh, "Metrics reset");
        return 0;
    }

    if (strcmp(argv[1], "history") == 0) {
        const auto &metrics = g_renderer_host->metrics();
        shell_print(sh, "\n=== Metrics History ===");
        shell_print(sh, "Total frames: %u", metrics.frame_count);
        shell_print(sh, "Total render time: %lld ms", metrics.accumulated_render_time);
        shell_print(sh, "Total write time: %lld ms", metrics.accumulated_write_time);
        shell_print(sh, "Avg render: %.1f ms", metrics.get_average_render_time());
        shell_print(sh, "Avg write: %.1f ms", metrics.get_average_write_time());
        shell_print(sh, "Avg total: %.1f ms",
                    (double)(metrics.accumulated_render_time + metrics.accumulated_write_time) /
                        metrics.frame_count);
        shell_print(sh, "Average FPS: %.1f", metrics.get_fps());
        shell_print(sh, "Min frame: %lld ms", metrics.min_frame_time_ms);
        shell_print(sh, "Max frame: %lld ms", metrics.max_frame_time_ms);
        return 0;
    }

    shell_print(sh, "Unknown subcommand: %s", argv[1]);
    return -EINVAL;
}

SHELL_STATIC_SUBCMD_SET_CREATE(slint_cmds,
                               SHELL_CMD(status, NULL, "Display Slint rendering status",
                                         cmd_slint_status),
                               SHELL_CMD(config, NULL, "Display driver configuration",
                                         cmd_slint_config),
                               SHELL_CMD(metrics, NULL,
                                         "Manage rendering metrics [reset|history]",
                                         cmd_slint_metrics),
                               SHELL_SUBCMD_SET_END);

SHELL_CMD_REGISTER(slint, &slint_cmds, "Slint monitoring commands", NULL);

void slint_shell_init(void)
{
    LOG_INF("Slint shell commands registered");
}
