#ifndef ZEPHYR_PLATFORM_MAIN_LOOP_H
#define ZEPHYR_PLATFORM_MAIN_LOOP_H

#include "display_backend.h"
#include "slint_renderer_host.h"
#include "input_bridge.h"
#include <slint-platform.h>
#include <zephyr/kernel.h>
#include <memory>
#include <deque>
#include <chrono>

/**
 * @brief Main event loop for Zephyr Slint platform
 * 
 * Implements Fase 1 and 5:
 * - Fase 1: Central loop managing all components without globals
 * - Fase 5: Smart wake-up based on multiple conditions
 * 
 * Centralizes wake reasons:
 * - Input events arrived
 * - Tasks queued
 * - Timers expired
 * - Redraw requested
 */
class ZephyrPlatformMainLoop : public slint::platform::Platform
{
public:
    /**
     * @brief Initialize platform with display device
     */
    explicit ZephyrPlatformMainLoop(const struct device *display);
    ~ZephyrPlatformMainLoop() = default;

    // Platform interface implementation
    std::unique_ptr<slint::platform::WindowAdapter> create_window_adapter() override;
    std::chrono::milliseconds duration_since_start() override;
    void run_event_loop() override;
    void quit_event_loop() override;
    void run_in_event_loop(Task task) override;

    /**
     * @brief Get renderer for metrics
     */
    SlintRendererHost *renderer() { return m_renderer.get(); }

    /**
     * @brief Get display backend for driver info
     */
    DisplayBackendZephyr *display() { return m_display_backend.get(); }

private:
    // Components (Fase 1 - no globals, clean separation)
    std::unique_ptr<DisplayBackendZephyr> m_display_backend;
    std::unique_ptr<SlintRendererHost> m_renderer;
    slint::platform::WindowAdapter *m_window = nullptr;

    // Task queue (protected by mutex)
    struct k_mutex m_queue_mutex;
    std::deque<Task> m_task_queue;
    bool m_quit_requested = false;

    // Event signaling (Fase 5 - wake reasons)
    struct k_sem m_event_sem;

    // State
    bool m_first_frame = true;  // Force render on first iteration
    static constexpr int64_t MIN_REDRAW_INTERVAL_MS = 33;  // ~30 FPS minimum

    // Helper methods
    void initialize_components();
    void process_queued_tasks();
    bool should_continue_loop();
    std::chrono::milliseconds calculate_sleep_duration() const;
};

#endif // ZEPHYR_PLATFORM_MAIN_LOOP_H
