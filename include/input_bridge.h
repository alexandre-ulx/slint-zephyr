#ifndef INPUT_BRIDGE_ZEPHYR_H
#define INPUT_BRIDGE_ZEPHYR_H

#include <slint.h>
#include <slint-platform.h>
#include <zephyr/kernel.h>
#include <optional>

/**
 * @brief Lightweight abstraction for Zephyr input events
 * 
 * Implements Fase 1, 4, and 5:
 * - Fase 1: Clean separation of input from event loop
 * - Fase 4: Thread-safe input handling without heavy logging in callback
 * - Fase 5: Minimal, lock-free input state management
 * 
 * Callback only:
 * - Normalizes events (touch press/move/release)
 * - Stores in ring buffer
 * - Signals main loop via semaphore
 * 
 * Main loop drains:
 * - Dequeues and dispatches events to Slint
 */
class InputBridgeZephyr
{
public:
    /**
     * @brief Initialize input bridge
     * @param sem Semaphore to signal when events arrive
     */
    static void init(struct k_sem *event_sem);

    /**
     * @brief Process single input event from Zephyr
     * Called from Zephyr input callback context
     */
    static void on_input_event(struct input_event *event);

    /**
     * @brief Drain and dispatch pending input events
     * Called from main loop
     */
    static void drain_events(slint::platform::WindowAdapter * window);

private:
    struct TouchState
    {
        slint::LogicalPosition pos;
        bool pressed = false;
        bool valid = false;  // Set when we have valid coordinates
    };

    struct InputEvent
    {
        enum class Type { Press, Move, Release };
        Type type;
        slint::LogicalPosition pos;
    };

    // Simple ring buffer for events
    static constexpr size_t EVENT_BUFFER_SIZE = 16;
    static InputEvent m_events[EVENT_BUFFER_SIZE];
    static size_t m_head;
    static size_t m_tail;

    // Touch state (last known position and press state)
    static TouchState m_touch_state;

    // Protect ring buffer from concurrent access
    static struct k_mutex m_input_mutex;

    /**
     * @brief Push event to ring buffer (called from callback, must be fast)
     */
    static bool push_event(const InputEvent &event);

    /**
     * @brief Pop event from ring buffer (called from main loop)
     */
    static std::optional<InputEvent> pop_event();

    /**
     * @brief Check if ring buffer has events
     */
    static bool has_events_locked();
};

#endif // INPUT_BRIDGE_ZEPHYR_H
