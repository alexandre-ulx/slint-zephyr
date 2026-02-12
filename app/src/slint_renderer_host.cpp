#include "slint_renderer_host.h"
#include <zephyr/cache.h>
#include <zephyr/logging/log.h>
#include <algorithm>
#include <ranges>
#include <limits>
#include <cstring>

LOG_MODULE_DECLARE(zephyr_api_slint, LOG_LEVEL_DBG);

SlintRendererHost::SlintRendererHost(DisplayBackendZephyr &display)
    : m_display(display), m_renderer(slint::platform::SoftwareRenderer::RepaintBufferType::ReusedBuffer)
{
    const auto &caps = m_display.get_capabilities();
    m_size = slint::PhysicalSize({ caps.width, caps.height });
}

bool SlintRendererHost::init()
{
    initialize_buffer();

    m_metrics.display_width = m_size.width;
    m_metrics.display_height = m_size.height;
    m_metrics.has_hardware_framebuffer = m_display.get_capabilities().has_framebuffer;
    m_metrics.using_framebuffer_direct = 
        m_display.get_capabilities().strategy == DisplayBackendZephyr::FrameBufferStrategy::DirectFrameBuffer;
    m_metrics.partial_updates_supported = 
        m_display.get_capabilities().supports_partial_updates;
    
    LOG_INF("Renderer initialized: %ux%u, framebuffer=%d, strategy=%d",
            m_size.width, m_size.height, m_metrics.has_hardware_framebuffer,
            m_metrics.using_framebuffer_direct);

    return true;
}

void SlintRendererHost::initialize_buffer()
{
    const auto &caps = m_display.get_capabilities();

    // Always keep a local buffer for SoftwareRenderer output.
    m_buffer.resize(m_size.width * m_size.height);
    m_buffer_descriptor.buf_size = sizeof(m_buffer[0]) * m_buffer.size();
    m_buffer_descriptor.width = m_size.width;
    m_buffer_descriptor.height = m_size.height;
    m_buffer_descriptor.pitch = m_size.width;

    m_framebuffer_ptr = nullptr;
    if (caps.strategy == DisplayBackendZephyr::FrameBufferStrategy::DirectFrameBuffer) {
        uint8_t *hw_fb = m_display.get_framebuffer();
        if (hw_fb) {
            m_framebuffer_ptr = reinterpret_cast<slint::platform::Rgb565Pixel*>(hw_fb);
            LOG_INF("DirectFrameBuffer mode: HW FB available (%ux%u)",
                    m_size.width, m_size.height);
        } else {
            LOG_WRN("DirectFrameBuffer strategy but no hardware FB available, using FullBuffer");
        }
    }

    m_metrics.buffer_size_bytes = m_buffer_descriptor.buf_size;
    LOG_INF("Render buffer: %u bytes (%ux%u pixels)",
            m_buffer_descriptor.buf_size, m_size.width, m_size.height);
}

int SlintRendererHost::render_and_present()
{
    if (!std::exchange(m_needs_redraw, false))
        return 0;

    m_metrics.reset_frame_metrics();

    // Measure Slint rendering time
    auto start = k_uptime_get();

    const auto &caps = m_display.get_capabilities();

    // Render to local buffer first.
    auto region = m_renderer.render(m_buffer, m_size.width);
    
    const auto &rects = region.rectangles();
    auto dirty_count = std::ranges::size(rects);

    m_metrics.slint_render_time_ms = k_uptime_delta(&start);
    m_metrics.num_dirty_rects = dirty_count;

    if (dirty_count == 0) {
        return 0;  // Nothing to update
    }

    // Calculate total dirty pixels
    uint32_t total_pixels = 0;
    for (auto [offset, size] : rects) {
        total_pixels += size.width * size.height;
        m_metrics.max_dirty_rect_width = std::max(m_metrics.max_dirty_rect_width, size.width);
        m_metrics.max_dirty_rect_height = std::max(m_metrics.max_dirty_rect_height, size.height);
    }
    m_metrics.total_dirty_pixels = total_pixels;

    // Collect dirty rects into vector
    std::vector<DirtyRect> dirty_rects;
    for (auto [offset, size] : rects) {
        dirty_rects.push_back(DirtyRect{(uint32_t)offset.x, (uint32_t)offset.y, size.width, size.height});
    }

    // Convert pixel format if needed (endian handling)
    if (caps.current_format == PIXEL_FORMAT_BGR_565) {
#ifndef CONFIG_SHIELD_RK055HDMIPI4MA0
        // Need to convert from RGB565 to BGR565 (big endian)
        // Apply to all dirty regions
        for (const auto &rect : dirty_rects) {
            convert_pixel_format_if_needed(rect.x, rect.y, rect.width, rect.height);
        }
        LOG_DBG("Applied BGR565 conversion for %u regions", (uint32_t)dirty_rects.size());
#endif
    }

    // For DirectFrameBuffer: copy to HW FB and flush cache
    if (caps.strategy == DisplayBackendZephyr::FrameBufferStrategy::DirectFrameBuffer && m_framebuffer_ptr) {
        uint32_t fb_size = m_size.width * m_size.height * sizeof(uint16_t);
        std::memcpy(m_framebuffer_ptr, m_buffer.data(), fb_size);
        sys_cache_data_flush_range(m_framebuffer_ptr, fb_size);
        LOG_DBG("DirectFrameBuffer: copied + flushed %u bytes", fb_size);
    }

    // Coalesce dirty rectangles if they overlap or are too many
    if (dirty_rects.size() > 4) {
        coalesce_dirty_rects(dirty_rects);
        LOG_DBG("Coalesced dirty rects to %u", (uint32_t)dirty_rects.size());
    }

    // For FullBuffer: send to display driver
    // For DirectFrameBuffer: no need to send (already copied to HW FB)
    if (caps.strategy != DisplayBackendZephyr::FrameBufferStrategy::DirectFrameBuffer) {
        auto write_start = k_uptime_get();
        send_to_display(dirty_rects, &m_buffer);
        m_metrics.display_write_time_ms = k_uptime_delta(&write_start);
    } else {
        m_metrics.display_write_time_ms = 0;
    }

    m_metrics.total_frame_time_ms = m_metrics.slint_render_time_ms + m_metrics.display_write_time_ms;
    m_metrics.update_frame_statistics();

    // Log performance every 60 frames
    if (m_metrics.frame_count % 60 == 0) {
        LOG_INF("Frame %u: render=%lld ms, write=%lld ms, fps=%.1f, dirty=%u/%u pixels",
                m_metrics.frame_count, m_metrics.slint_render_time_ms,
                m_metrics.display_write_time_ms, m_metrics.get_fps(),
                m_metrics.num_dirty_rects, m_metrics.total_dirty_pixels);
    }

    return 1;  // Always return 1 to indicate work was done
}

void SlintRendererHost::convert_pixel_format_if_needed(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
    if (m_buffer.empty())
        return;

    // Convert endianness for BGR565 format
    // This avoids per-pixel conversion in the hot path by doing block conversion
    for (uint32_t row = y; row < y + height; row++) {
        for (uint32_t col = x; col < x + width; col++) {
            if (row * m_size.width + col >= m_buffer.size())
                continue;

            auto px = reinterpret_cast<uint16_t *>(&m_buffer[row * m_size.width + col]);
            // Swap bytes: little endian to big endian
            *px = (*px << 8) | (*px >> 8);
        }
    }
}

void SlintRendererHost::coalesce_dirty_rects(std::vector<DirtyRect> &rects)
{
    if (rects.size() <= 1)
        return;

    // Simple coalescing: find bounds that contain all rects
    uint32_t min_x = std::numeric_limits<uint32_t>::max();
    uint32_t min_y = std::numeric_limits<uint32_t>::max();
    uint32_t max_x = 0;
    uint32_t max_y = 0;

    for (const auto &rect : rects) {
        min_x = std::min(min_x, rect.x);
        min_y = std::min(min_y, rect.y);
        max_x = std::max(max_x, rect.x + rect.width);
        max_y = std::max(max_y, rect.y + rect.height);
    }

    rects.clear();
    rects.push_back(DirtyRect{min_x, min_y, max_x - min_x, max_y - min_y});
}

int SlintRendererHost::send_to_display(const std::vector<DirtyRect> &rects, 
                                         const std::vector<slint::platform::Rgb565Pixel> *buffer)
{
    if (!buffer || buffer->empty()) {
        return -ENOMEM;
    }

    const auto &caps = m_display.get_capabilities();
    const uint16_t *src_pixels = reinterpret_cast<const uint16_t*>(buffer->data());

    // DirectFrameBuffer: update only dirty regions directly in HW FB
    if (caps.strategy == DisplayBackendZephyr::FrameBufferStrategy::DirectFrameBuffer && m_framebuffer_ptr) {
        uint16_t *dst_pixels = reinterpret_cast<uint16_t*>(m_framebuffer_ptr);

        for (const auto &rect : rects) {
            if (rect.x + rect.width > m_size.width || rect.y + rect.height > m_size.height) {
                continue;
            }

            const uint32_t bytes_per_row = rect.width * sizeof(uint16_t);
            for (uint32_t row = 0; row < rect.height; row++) {
                const uint32_t src_offset = (rect.y + row) * m_size.width + rect.x;
                const uint32_t dst_offset = src_offset;
                std::memcpy(dst_pixels + dst_offset, src_pixels + src_offset, bytes_per_row);
                sys_cache_data_flush_range(dst_pixels + dst_offset, bytes_per_row);
            }
        }

        return rects.empty() ? 0 : static_cast<int>(rects.size());
    }

    // FullBuffer: prefer partial updates if supported
    if (!caps.supports_partial_updates || rects.empty()) {
        return m_display.write_full_frame(src_pixels);
    }

    int total_writes = 0;
    for (const auto &rect : rects) {
        if (rect.x + rect.width > m_size.width || rect.y + rect.height > m_size.height) {
            continue;
        }

        const uint32_t pixel_count = rect.width * rect.height;
        m_region_buffer.resize(pixel_count);

        for (uint32_t row = 0; row < rect.height; row++) {
            const uint32_t src_offset = (rect.y + row) * m_size.width + rect.x;
            const uint32_t dst_offset = row * rect.width;
            std::memcpy(m_region_buffer.data() + dst_offset, src_pixels + src_offset,
                        rect.width * sizeof(uint16_t));
        }

        if (m_display.write_region(rect.x, rect.y, rect.width, rect.height,
                                   m_region_buffer.data()) == 0) {
            total_writes++;
        }
    }

    return total_writes;
}
