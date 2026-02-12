#include "platform_main_loop.h"
#include "slint_shell.h"
#include <zephyr/logging/log.h>

LOG_MODULE_DECLARE(zephyr_api_slint, LOG_LEVEL_DBG);

ZephyrPlatformMainLoop::ZephyrPlatformMainLoop(const struct device *display)
{
    k_mutex_init(&m_queue_mutex);
    k_sem_init(&m_event_sem, 0, 1);

    m_display_backend = std::make_unique<DisplayBackendZephyr>(display);
    if (!m_display_backend->init()) {
        LOG_ERR("Failed to initialize display backend");
        m_display_backend.reset();
        return;
    }

    m_renderer = std::make_unique<SlintRendererHost>(*m_display_backend);
    if (!m_renderer->init()) {
        LOG_ERR("Failed to initialize renderer");
        m_renderer.reset();
        return;
    }

    // Initialize input bridge with event semaphore
    InputBridgeZephyr::init(&m_event_sem);

    // Register components for shell monitoring
    slint_shell_register_renderer(m_renderer.get());
    slint_shell_register_display(m_display_backend.get());

    LOG_INF("ZephyrPlatformMainLoop initialized successfully");
}

std::unique_ptr<slint::platform::WindowAdapter> ZephyrPlatformMainLoop::create_window_adapter()
{
    LOG_INF("create_window_adapter() called");
    
    if (!m_display_backend || !m_renderer) {
        LOG_ERR("Components not initialized");
        return nullptr;
    }

    if (m_window) {
        LOG_ERR("Window adapter already created");
        return nullptr;
    }

    // Create window adapter that uses our renderer
    class WindowAdapter : public slint::platform::WindowAdapter
    {
    public:
        explicit WindowAdapter(ZephyrPlatformMainLoop *platform) : m_platform(platform)
        {
        }

        void request_redraw() override
        {
            if (m_platform->m_renderer) {
                m_platform->m_renderer->request_redraw();
            }
            k_sem_give(&m_platform->m_event_sem);
        }

        slint::PhysicalSize size() override
        {
            if (m_platform->m_renderer) {
                return m_platform->m_renderer->size();
            }
            return slint::PhysicalSize({ 0, 0 });
        }

        slint::platform::AbstractRenderer &renderer() override
        {
            return m_platform->m_renderer->renderer();
        }

    private:
        ZephyrPlatformMainLoop *m_platform;
    };

    auto window = std::make_unique<WindowAdapter>(this);
    m_window = window.get();

    LOG_INF("Window adapter created: %ux%u",
            m_renderer->size().width, m_renderer->size().height);

    return window;
}

std::chrono::milliseconds ZephyrPlatformMainLoop::duration_since_start()
{
    return std::chrono::milliseconds(k_uptime_get());
}

void ZephyrPlatformMainLoop::run_event_loop()
{
    LOG_INF("Event loop starting - m_window=%p, m_renderer=%p", (void*)m_window, (void*)m_renderer.get());

    if (!m_window) {
        LOG_ERR("ERROR: m_window is null! create_window_adapter() was not called properly");
        return;
    }

    if (!m_renderer) {
        LOG_ERR("ERROR: m_renderer is null!");
        return;
    }

    while (should_continue_loop()) {
        // Process all wake sources
        
        // 1. Process queued tasks
        process_queued_tasks();

        // 2. Drain input events with window dispatch
        if (m_window) {
            InputBridgeZephyr::drain_events(m_window);
        }

        // 3. Update animations and timers
        slint::platform::update_timers_and_animations();

        // 4. Render and present if needed
        // On first frame, always render even without redraw request
        bool should_render = m_first_frame || m_renderer->needs_redraw();
        if (m_first_frame) {
            m_first_frame = false;
            LOG_INF("First frame rendering starting");
        }
        
        if (m_window && m_renderer && should_render) {
            m_renderer->render_and_present();
        }

        // 5. Decide sleep duration (Fase 5 - sleep policy)
        auto sleep_time = calculate_sleep_duration();

        if (sleep_time.count() > 0) {
            k_sem_take(&m_event_sem, K_MSEC(sleep_time.count()));
        } else if (m_window && m_window->window().has_active_animations()) {
            // Has active animations: sleep very little to check next frame
            LOG_INF("Has animations, sleeping 1ms");
#if defined(CONFIG_ARCH_POSIX)
            k_sem_take(&m_event_sem, K_MSEC(10));
#else
            k_sem_take(&m_event_sem, K_MSEC(1));
#endif
        } else {
            // No animations: wait with heartbeat timeout to ensure minimum redraw rate
            // LOG_INF("Waiting for event (max %lld ms)", MIN_REDRAW_INTERVAL_MS);
            k_sem_take(&m_event_sem, K_MSEC(MIN_REDRAW_INTERVAL_MS));
        }
    }

    LOG_INF("Event loop ending");
}

void ZephyrPlatformMainLoop::quit_event_loop()
{
    {
        k_mutex_lock(&m_queue_mutex, K_FOREVER);
        m_quit_requested = true;
        k_mutex_unlock(&m_queue_mutex);
    }
    k_sem_give(&m_event_sem);
}

void ZephyrPlatformMainLoop::run_in_event_loop(Task task)
{
    {
        k_mutex_lock(&m_queue_mutex, K_FOREVER);
        m_task_queue.push_back(std::move(task));
        k_mutex_unlock(&m_queue_mutex);
    }
    k_sem_give(&m_event_sem);
}

void ZephyrPlatformMainLoop::process_queued_tasks()
{
    std::optional<Task> task;

    while (true) {
        {
            k_mutex_lock(&m_queue_mutex, K_FOREVER);
            if (m_task_queue.empty()) {
                k_mutex_unlock(&m_queue_mutex);
                return;
            }
            task = std::move(m_task_queue.front());
            m_task_queue.pop_front();
            k_mutex_unlock(&m_queue_mutex);
        }

        if (task) {
            LOG_DBG("Executing queued task");
            std::move(*task).run();
            task.reset();
        }
    }
}

bool ZephyrPlatformMainLoop::should_continue_loop()
{
    k_mutex_lock(&m_queue_mutex, K_FOREVER);
    bool should_quit = m_quit_requested;
    if (should_quit) {
        m_quit_requested = false;  // Reset for next run
    }
    k_mutex_unlock(&m_queue_mutex);

    // LOG_DBG("should_continue_loop: quit=%d, continue=%d", should_quit, !should_quit);
    return !should_quit;
}

std::chrono::milliseconds ZephyrPlatformMainLoop::calculate_sleep_duration() const
{
    // Central sleep policy
    // 
    // If window has animations, don't sleep long
    if (m_window && m_window->window().has_active_animations()) {
        return std::chrono::milliseconds(0);  // Don't sleep, check next frame soon
    }

    // Get next timer deadline from Slint
    if (auto next_timer = slint::platform::duration_until_next_timer_update()) {
        return next_timer.value();
    }

    // No timers - return 0 to allow loop to continue
    // The k_sem_take in run_event_loop will timeout after MIN_REDRAW_INTERVAL_MS
    return std::chrono::milliseconds(0);
}
