#ifndef RENDERING_METRICS_H
#define RENDERING_METRICS_H

#include <cstdint>
#include <cstring>

/**
 * @brief Structure to collect rendering and display metrics
 * 
 * Used for Phase 0 (baseline measurement):
 * - Slint render time
 * - Display write time
 * - Redraw rate (FPS)
 * - RAM usage (buffer size, peaks)
 * - Number of dirty rectangles
 * - Sum of dirty rectangle areas
 */
struct RenderingMetrics
{
    // Timing metrics (in milliseconds)
    int64_t slint_render_time_ms = 0;
    int64_t display_write_time_ms = 0;
    int64_t total_frame_time_ms = 0;

    // Dirty region metrics
    uint32_t num_dirty_rects = 0;
    uint32_t total_dirty_pixels = 0;
    uint32_t max_dirty_rect_width = 0;
    uint32_t max_dirty_rect_height = 0;

    // Display metrics
    uint32_t display_width = 0;
    uint32_t display_height = 0;
    uint32_t framebuffer_size_bytes = 0;
    uint32_t buffer_size_bytes = 0;
    bool has_hardware_framebuffer = false;
    bool using_framebuffer_direct = false;
    bool partial_updates_supported = false;

    // Statistics
    uint32_t frame_count = 0;
    int64_t accumulated_render_time = 0;
    int64_t accumulated_write_time = 0;
    int64_t min_frame_time_ms = INT64_MAX;
    int64_t max_frame_time_ms = 0;

    // Methods for logging and analysis
    void reset_frame_metrics()
    {
        slint_render_time_ms = 0;
        display_write_time_ms = 0;
        total_frame_time_ms = 0;
        num_dirty_rects = 0;
        total_dirty_pixels = 0;
        max_dirty_rect_width = 0;
        max_dirty_rect_height = 0;
    }

    void update_frame_statistics()
    {
        frame_count++;
        accumulated_render_time += slint_render_time_ms;
        accumulated_write_time += display_write_time_ms;
        
        if (total_frame_time_ms < min_frame_time_ms)
            min_frame_time_ms = total_frame_time_ms;
        if (total_frame_time_ms > max_frame_time_ms)
            max_frame_time_ms = total_frame_time_ms;
    }

    double get_average_render_time() const
    {
        return frame_count > 0 ? (double)accumulated_render_time / frame_count : 0;
    }

    double get_average_write_time() const
    {
        return frame_count > 0 ? (double)accumulated_write_time / frame_count : 0;
    }

    double get_fps() const
    {
        return frame_count > 0 ? (1000.0 * frame_count) / accumulated_render_time : 0;
    }
};

#endif // RENDERING_METRICS_H
