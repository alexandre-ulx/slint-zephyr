#ifndef DISPLAY_BACKEND_ZEPHYR_H
#define DISPLAY_BACKEND_ZEPHYR_H

#include <cstdint>
#include <memory>
#include <vector>
#include <zephyr/drivers/display.h>
#include <zephyr/autoconf.h>
#include <slint.h>

/**
 * @brief Abstraction layer for Zephyr display driver
 * 
 * Encapsulates:
 * - Display capabilities detection
 * - Pixel format negotiation
 * - Framebuffer access (if available)
 * - Display write operations
 * 
 * Provides clean interface for rendering backend to communicate with display driver
 * without dealing directly with Zephyr APIs.
 */
class DisplayBackendZephyr
{
public:
    enum class FrameBufferStrategy
    {
        DirectFrameBuffer,  // Use driver's framebuffer directly
        FullBuffer,         // Maintain full buffer and copy to driver (fallback)
        PartialUpdates,     // Support partial region updates
    };

    struct Capabilities
    {
        uint32_t width = 0;
        uint32_t height = 0;
        bool has_framebuffer = false;
        bool supports_partial_updates = false;
        bool double_buffering = false;
        display_pixel_format current_format = PIXEL_FORMAT_RGB_565;
        FrameBufferStrategy strategy = FrameBufferStrategy::FullBuffer;
    };

    explicit DisplayBackendZephyr(const struct device *display);
    ~DisplayBackendZephyr() = default;

    // Non-copyable, non-movable for simplicity
    DisplayBackendZephyr(const DisplayBackendZephyr &) = delete;
    DisplayBackendZephyr &operator=(const DisplayBackendZephyr &) = delete;
    DisplayBackendZephyr(DisplayBackendZephyr &&) = delete;
    DisplayBackendZephyr &operator=(DisplayBackendZephyr &&) = delete;

    /**
     * @brief Initialize and query display capabilities
     * @return True if initialization successful
     */
    bool init();

    /**
     * @brief Get current display capabilities
     */
    const Capabilities &get_capabilities() const { return m_capabilities; }

    /**
     * @brief Get pointer to display device
     */
    const struct device *get_device() const { return m_display; }

    /**
     * @brief Attempt to set pixel format
     * @param format Target pixel format
     * @return True if successfully set
     */
    bool set_pixel_format(display_pixel_format format);

    /**
     * @brief Get direct framebuffer pointer if available
     * @return Pointer to framebuffer or nullptr if not available
     */
    uint8_t *get_framebuffer() const;

    /**
     * @brief Write buffer region to display
     * @param x Starting x coordinate
     * @param y Starting y coordinate
     * @param width Width of region
     * @param height Height of region
     * @param buffer Pixel data (must match format)
     * @return 0 on success, non-zero on error
     */
    int write_region(uint32_t x, uint32_t y, uint32_t width, uint32_t height,
                     const void *buffer);

    /**
     * @brief Write full framebuffer to display
     * @param buffer Pixel data covering entire display
     * @return 0 on success, non-zero on error
     */
    int write_full_frame(const void *buffer);

    /**
     * @brief Enable/disable display blank
     */
    void blanking_off() const;

private:
    const struct device *m_display;
    Capabilities m_capabilities;

    /**
     * @brief Detect optimal rendering strategy based on driver capabilities
     */
    void detect_strategy();

    /**
     * @brief Check if pixel format is supported by Slint
     */
    bool is_supported_pixel_format(display_pixel_format format) const;

    /**
     * @brief Log detected capabilities
     */
    void log_capabilities();
};

#endif // DISPLAY_BACKEND_ZEPHYR_H
