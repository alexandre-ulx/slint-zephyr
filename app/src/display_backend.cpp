#include "display_backend.h"
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(zephyr_api_slint, LOG_LEVEL_DBG);

DisplayBackendZephyr::DisplayBackendZephyr(const struct device *display) : m_display(display)
{
}

bool DisplayBackendZephyr::init()
{
    if (!m_display) {
        LOG_ERR("Display device is null");
        return false;
    }

    display_capabilities caps;
    display_get_capabilities(m_display, &caps);

    m_capabilities.width = caps.x_resolution;
    m_capabilities.height = caps.y_resolution;
    m_capabilities.current_format = caps.current_pixel_format;
    m_capabilities.has_framebuffer = (display_get_framebuffer(m_display) != nullptr);
    m_capabilities.double_buffering = (caps.screen_info & SCREEN_INFO_DOUBLE_BUFFER) != 0;

    // Try to set RGB565 if not already set
    if (!is_supported_pixel_format(m_capabilities.current_format)) {
        if (caps.supported_pixel_formats & PIXEL_FORMAT_RGB_565) {
            LOG_INF("Negotiating pixel format to RGB_565");
            if (set_pixel_format(PIXEL_FORMAT_RGB_565)) {
                m_capabilities.current_format = PIXEL_FORMAT_RGB_565;
            }
        } else if (caps.supported_pixel_formats & PIXEL_FORMAT_BGR_565) {
            LOG_INF("Negotiating pixel format to BGR_565");
            if (set_pixel_format(PIXEL_FORMAT_BGR_565)) {
                m_capabilities.current_format = PIXEL_FORMAT_BGR_565;
            }
        } else {
            LOG_ERR("No supported pixel formats found!");
            return false;
        }
    }

    detect_strategy();
    log_capabilities();
    return true;
}

bool DisplayBackendZephyr::is_supported_pixel_format(display_pixel_format format) const
{
    switch (format) {
    case PIXEL_FORMAT_RGB_565:
        return true;
    case PIXEL_FORMAT_BGR_565:
#ifdef CONFIG_SHIELD_RK055HDMIPI4MA0
        return true;
#else
        return false;
#endif
    case PIXEL_FORMAT_RGB_888:
    case PIXEL_FORMAT_MONO01:
    case PIXEL_FORMAT_MONO10:
    case PIXEL_FORMAT_ARGB_8888:
        return false;
    }
    return false;
}

bool DisplayBackendZephyr::set_pixel_format(display_pixel_format format)
{
    display_set_pixel_format(m_display, format);

    display_capabilities caps;
    display_get_capabilities(m_display, &caps);

    return true;
}

uint8_t *DisplayBackendZephyr::get_framebuffer() const
{
    if (m_capabilities.has_framebuffer) {
        return reinterpret_cast<uint8_t *>(display_get_framebuffer(m_display));
    }
    return nullptr;
}

int DisplayBackendZephyr::write_region(uint32_t x, uint32_t y, uint32_t width, uint32_t height,
                                       const void *buffer)
{
    // Region writes only supported for FullBuffer strategy
    // DirectFrameBuffer should always use full-frame writes to avoid stride confusion
    if (m_capabilities.strategy == FrameBufferStrategy::DirectFrameBuffer) {
        // Ignore region writes for DirectFrameBuffer
        LOG_DBG("write_region: ignored for DirectFrameBuffer (use full-frame)");
        return 0;
    }

    display_buffer_descriptor desc{};
    desc.width = width;
    desc.height = height;
    desc.pitch = width;
    desc.buf_size = width * height * sizeof(uint16_t);

    return display_write(m_display, x, y, &desc, buffer);
}

int DisplayBackendZephyr::write_full_frame(const void *buffer)
{
    // Both strategies use full-frame write for maximum compatibility
    // write_full_frame knows how to handle the full buffer correctly
    
    display_buffer_descriptor desc{};
    desc.width = m_capabilities.width;
    desc.height = m_capabilities.height;
    desc.pitch = m_capabilities.width;
    desc.buf_size = m_capabilities.width * m_capabilities.height * sizeof(uint16_t);

    LOG_DBG("write_full_frame: %ux%u, pitch=%u", m_capabilities.width, m_capabilities.height, desc.pitch);

    return display_write(m_display, 0, 0, &desc, buffer);
}

void DisplayBackendZephyr::blanking_off() const
{
    display_blanking_off(m_display);
}

void DisplayBackendZephyr::detect_strategy()
{
    // Use DirectFrameBuffer if hardware supports it
    if (m_capabilities.has_framebuffer) {
        m_capabilities.strategy = FrameBufferStrategy::DirectFrameBuffer;
        m_capabilities.supports_partial_updates = true;  // Can optimize regions later
        LOG_INF("Display strategy: Direct Framebuffer (HW available)");
        return;
    }

    // Fallback to FullBuffer (always safe)
    m_capabilities.strategy = FrameBufferStrategy::FullBuffer;

    // Check if driver supports partial updates (e.g., not PXP)
#ifdef CONFIG_MCUX_ELCDIF_PXP
    m_capabilities.supports_partial_updates = false;
    LOG_INF("Display strategy: Full Buffer (PXP forces full frame)");
#else
    m_capabilities.supports_partial_updates = true;
    LOG_INF("Display strategy: Full Buffer with Partial Updates");
#endif
}

void DisplayBackendZephyr::log_capabilities()
{
    LOG_INF("=== Display Capabilities ===");
    LOG_INF("Resolution: %u x %u", m_capabilities.width, m_capabilities.height);
    LOG_INF("Pixel Format: %u (RGB565=%d, BGR565=%d)", m_capabilities.current_format,
            PIXEL_FORMAT_RGB_565, PIXEL_FORMAT_BGR_565);
    LOG_INF("Has Framebuffer: %d", m_capabilities.has_framebuffer);
    LOG_INF("Double Buffering: %d", m_capabilities.double_buffering);
    LOG_INF("Supports Partial Updates: %d", m_capabilities.supports_partial_updates);
    LOG_INF("Rendering Strategy: %d", static_cast<int>(m_capabilities.strategy));
}
