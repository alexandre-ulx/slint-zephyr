#include "input_bridge.h"
#include <zephyr/logging/log.h>
#include <zephyr/input/input.h>

LOG_MODULE_DECLARE(zephyr_api_slint, LOG_LEVEL_DBG);

// Static member initialization
InputBridgeZephyr::InputEvent InputBridgeZephyr::m_events[InputBridgeZephyr::EVENT_BUFFER_SIZE];
size_t InputBridgeZephyr::m_head = 0;
size_t InputBridgeZephyr::m_tail = 0;
InputBridgeZephyr::TouchState InputBridgeZephyr::m_touch_state;
K_MUTEX_DEFINE(InputBridgeZephyr::m_input_mutex);

static struct k_sem *g_event_sem = nullptr;

void InputBridgeZephyr::init(struct k_sem *event_sem)
{
    g_event_sem = event_sem;
    m_head = 0;
    m_tail = 0;
    m_touch_state = {};
    LOG_DBG("InputBridgeZephyr initialized");
}

void InputBridgeZephyr::on_input_event(struct input_event *event)
{
    if (!event || !g_event_sem)
        return;

    // Callback must be fast - no heavy logging, just state update

    // Normalize and accumulate position
    if (event->code == INPUT_ABS_X) {
        m_touch_state.pos.x = event->value;
    } else if (event->code == INPUT_ABS_Y) {
        m_touch_state.pos.y = event->value;
    } else if (event->code != INPUT_BTN_TOUCH) {
        return;
    }

    // When sync happens on BTN_TOUCH, we have a complete event
    if (event->sync && event->code == INPUT_BTN_TOUCH) {
        m_touch_state.valid = true;

        InputEvent input_evt;
        input_evt.pos = m_touch_state.pos;

        bool press_state_changed = (m_touch_state.pressed != (event->value != 0));

        if (press_state_changed) {
            m_touch_state.pressed = (event->value != 0);

            if (m_touch_state.pressed) {
                input_evt.type = InputEvent::Type::Press;
            } else {
                input_evt.type = InputEvent::Type::Release;
            }

            // Minimal logging - only in DBG level
            LOG_DBG("Input: %s at (%.0f, %.0f)", 
                    m_touch_state.pressed ? "Press" : "Release",
                    m_touch_state.pos.x, m_touch_state.pos.y);
        } else if (m_touch_state.pressed) {
            // Still pressed, this is a move event
            input_evt.type = InputEvent::Type::Move;
            LOG_DBG("Input: Move at (%.0f, %.0f)", m_touch_state.pos.x, m_touch_state.pos.y);
        } else {
            return;  // Release when already released = skip
        }

        // Push event to buffer (fast path, minimal lock time)
        if (push_event(input_evt)) {
            // Signal main loop that events are ready
            k_sem_give(g_event_sem);
        }
    }
}

void InputBridgeZephyr::drain_events(slint::platform::WindowAdapter * window)
{
    if (!window)
        return;

    std::optional<InputEvent> evt;
    while ((evt = pop_event()).has_value()) {
        auto &event = evt.value();
        auto &slint_window = window->window();

        switch (event.type) {
        case InputEvent::Type::Press:
            LOG_DBG("Dispatching Press event");
            slint_window.dispatch_pointer_move_event(event.pos);
            slint_window.dispatch_pointer_press_event(event.pos,
                                                       slint::PointerEventButton::Left);
            break;

        case InputEvent::Type::Move:
            LOG_DBG("Dispatching Move event");
            slint_window.dispatch_pointer_move_event(event.pos);
            break;

        case InputEvent::Type::Release:
            LOG_DBG("Dispatching Release event");
            slint_window.dispatch_pointer_release_event(event.pos,
                                                         slint::PointerEventButton::Left);
            slint_window.dispatch_pointer_exit_event();
            break;
        }
    }
}

bool InputBridgeZephyr::push_event(const InputEvent &event)
{
    k_mutex_lock(&m_input_mutex, K_FOREVER);

    size_t next_head = (m_head + 1) % EVENT_BUFFER_SIZE;
    if (next_head == m_tail) {
        // Buffer full - drop oldest event
        m_tail = (m_tail + 1) % EVENT_BUFFER_SIZE;
        LOG_WRN("Input event buffer full, dropping oldest");
    }

    m_events[m_head] = event;
    m_head = next_head;

    k_mutex_unlock(&m_input_mutex);
    return true;
}

std::optional<InputBridgeZephyr::InputEvent> InputBridgeZephyr::pop_event()
{
    k_mutex_lock(&m_input_mutex, K_FOREVER);

    if (m_head == m_tail) {
        k_mutex_unlock(&m_input_mutex);
        return std::nullopt;
    }

    InputEvent evt = m_events[m_tail];
    m_tail = (m_tail + 1) % EVENT_BUFFER_SIZE;

    k_mutex_unlock(&m_input_mutex);
    return evt;
}

bool InputBridgeZephyr::has_events_locked()
{
    return m_head != m_tail;
}

// Zephyr input callback - registered globally
static void zephyr_input_callback(struct input_event *event, void *user_data)
{
    ARG_UNUSED(user_data);
    InputBridgeZephyr::on_input_event(event);
}

INPUT_CALLBACK_DEFINE(DEVICE_DT_GET(DT_CHOSEN(zephyr_touch)), zephyr_input_callback, NULL);
