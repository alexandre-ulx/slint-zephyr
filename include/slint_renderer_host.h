#ifndef SLINT_RENDERER_HOST_H
#define SLINT_RENDERER_HOST_H

#include "display_backend.h"
#include "rendering_metrics.h"
#include <slint-platform.h>
#include <memory>
#include <vector>
#include <zephyr/kernel.h>

/**
 * @brief Simple rectangle structure for dirty region tracking
 */
struct DirtyRect
{
    uint32_t x = 0;
    uint32_t y = 0;
    uint32_t width = 0;
    uint32_t height = 0;
};

/**
 * @brief Manages Slint SoftwareRenderer with display strategy
 * 
 * Handles:
 * - Renderer initialization
 * - Buffer management (framebuffer direct or full buffer)
 * - Rendering and dirty region optimization
 * - Pixel format conversion (endian handling) - Fase 3
 * - Rectangle coalescing for partial updates - Fase 3
 * - Metrics collection - Fase 0
 */
class SlintRendererHost
{
public:
    explicit SlintRendererHost(DisplayBackendZephyr &display);
    ~SlintRendererHost() = default;

    // Non-copyable
    SlintRendererHost(const SlintRendererHost &) = delete;
    SlintRendererHost &operator=(const SlintRendererHost &) = delete;

    /**
     * @brief Initialize renderer
     * @return True if successful
     */
    bool init();

    /**
     * @brief Get the Slint software renderer
     */
    slint::platform::SoftwareRenderer &renderer() { return m_renderer; }

    /**
     * @brief Get physical display size
     */
    slint::PhysicalSize size() const { return m_size; }

    /**
     * @brief Render current frame and send to display
     * @return Number of dirty regions rendered, or -1 on error
     */
    int render_and_present();

    /**
     * @brief Check if rendering is needed
     */
    bool needs_redraw() const { return m_needs_redraw; }

    /**
     * @brief Request redraw
     */
    void request_redraw() { m_needs_redraw = true; }

    /**
     * @brief Get current rendering metrics
     */
    const RenderingMetrics &metrics() const { return m_metrics; }

    /**
     * @brief Get mutable metrics for updating
     */
    RenderingMetrics &metrics() { return m_metrics; }

private:
    DisplayBackendZephyr &m_display;
    slint::platform::SoftwareRenderer m_renderer;
    slint::PhysicalSize m_size;

    bool m_needs_redraw = true;

    // Buffer management (Fase 2A/2B)
    std::vector<slint::platform::Rgb565Pixel> m_buffer;
    slint::platform::Rgb565Pixel *m_framebuffer_ptr = nullptr;  // Hardware FB when available
    std::vector<uint16_t> m_region_buffer;  // Reused scratch buffer for region writes
    display_buffer_descriptor m_buffer_descriptor{};

    // Metrics collection (Fase 0)
    RenderingMetrics m_metrics;

    // Helper methods
    void initialize_buffer();
    void convert_pixel_format_if_needed(uint32_t x, uint32_t y, uint32_t width, uint32_t height);
    void coalesce_dirty_rects(std::vector<DirtyRect> &rects);
    int send_to_display(const std::vector<DirtyRect> &rects, 
                       const std::vector<slint::platform::Rgb565Pixel> *buffer);
};

#endif // SLINT_RENDERER_HOST_H
