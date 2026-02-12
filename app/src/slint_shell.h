#ifndef SLINT_SHELL_H
#define SLINT_SHELL_H

// Forward declarations
class SlintRendererHost;
class DisplayBackendZephyr;

/**
 * @brief Initialize Slint monitoring shell commands
 * 
 * Provides shell commands for:
 * - slint status: Display current rendering metrics and driver info
 * - slint metrics: Manage metrics (reset, history)
 * - slint config: Show display driver configuration
 */
void slint_shell_init(void);

/**
 * @brief Register renderer for shell access
 */
void slint_shell_register_renderer(SlintRendererHost *renderer);

/**
 * @brief Register display backend for shell access
 */
void slint_shell_register_display(DisplayBackendZephyr *display);

#endif // SLINT_SHELL_H
