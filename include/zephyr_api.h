#ifndef ZEPHYR_API_H
#define ZEPHYR_API_H

#ifdef __cplusplus
extern "C" {
#endif

void slint_zephyr_init(const struct device *display);

#ifdef __cplusplus
}
#endif

#endif // ZEPHYR_API_H