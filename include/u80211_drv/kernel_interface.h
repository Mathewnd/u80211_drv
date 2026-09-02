#ifndef U80211_DRV_KERNEL_INTERFACE_H
#define U80211_DRV_KERNEL_INTERFACE_H

#include <stddef.h>
#include <stdint.h>

typedef void *u80211_drv_device_handle_t;
typedef void *u80211_drv_interface_handle_t;
typedef void *u80211_drv_endpoint_handle_t;

typedef struct {
	uint16_t vendor_id;
	uint16_t product_id;
} u80211_drv_device_descriptor_t;

typedef struct {
	uint8_t number;
	uint8_t class_code;
	uint8_t subclass;
	uint8_t protocol;
	uint8_t endpoint_count;
} u80211_drv_interface_descriptor_t;

typedef struct {
	uint8_t address;
	uint8_t attributes;
	uint16_t maximum_packet_size;
	uint8_t interval;
} u80211_drv_endpoint_descriptor_t;

#define U80211_DRV_KERNEL_PRINT_LEVEL_INFO 0
#define U80211_DRV_KERNEL_PRINT_LEVEL_WARN 1
#define U80211_DRV_KERNEL_PRINT_LEVEL_ERROR 2

void *u80211_drv_kernel_allocate(size_t size);
void u80211_drv_kernel_free(void *memory);
int u80211_drv_kernel_get_device_descriptor(u80211_drv_device_handle_t device, u80211_drv_device_descriptor_t *descriptor);
int u80211_drv_kernel_get_interface_descriptor(u80211_drv_interface_handle_t interface, u80211_drv_interface_descriptor_t *descriptor);
int u80211_drv_kernel_get_endpoints(u80211_drv_interface_handle_t interface, u80211_drv_endpoint_handle_t *endpoints, size_t endpoint_count);
int u80211_drv_kernel_get_endpoint_descriptor(u80211_drv_endpoint_handle_t endpoint, u80211_drv_endpoint_descriptor_t *descriptor);
void u80211_drv_kernel_release_endpoint(u80211_drv_endpoint_handle_t endpoint);
void u80211_drv_kernel_print(int level, const char *msg);

#endif
