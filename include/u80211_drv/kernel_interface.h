#ifndef U80211_DRV_KERNEL_INTERFACE_H
#define U80211_DRV_KERNEL_INTERFACE_H

#include <stdint.h>

typedef void *u80211_drv_device_handle_t;
typedef void *u80211_drv_interface_handle_t;

typedef struct {
	uint16_t vendor_id;
	uint16_t product_id;
} u80211_drv_device_descriptor_t;

typedef struct {
	uint8_t number;
	uint8_t class_code;
	uint8_t subclass;
	uint8_t protocol;
} u80211_drv_interface_descriptor_t;

#define U80211_DRV_KERNEL_PRINT_LEVEL_INFO 0
#define U80211_DRV_KERNEL_PRINT_LEVEL_WARN 1
#define U80211_DRV_KERNEL_PRINT_LEVEL_ERROR 2

int u80211_drv_kernel_get_device_descriptor(u80211_drv_device_handle_t device, u80211_drv_device_descriptor_t *descriptor);
int u80211_drv_kernel_get_interface_descriptor(u80211_drv_interface_handle_t interface, u80211_drv_interface_descriptor_t *descriptor);
void u80211_drv_kernel_print(int level, const char *msg);

#endif
