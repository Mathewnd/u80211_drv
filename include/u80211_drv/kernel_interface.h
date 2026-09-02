#ifndef U80211_DRV_KERNEL_INTERFACE_H
#define U80211_DRV_KERNEL_INTERFACE_H

#include <stddef.h>
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
	uint8_t endpoint_count;
} u80211_drv_interface_descriptor_t;

typedef struct {
	uint8_t address;
	uint8_t attributes;
	uint16_t maximum_packet_size;
	uint8_t interval;
} u80211_drv_endpoint_descriptor_t;

#define U80211_DRV_KERNEL_XFER_OUT 0x00
#define U80211_DRV_KERNEL_XFER_IN 0x80
#define U80211_DRV_KERNEL_XFER_DIRECTION_MASK 0x80

#define U80211_DRV_KERNEL_XFER_REQUEST_TYPE_STANDARD 0x00
#define U80211_DRV_KERNEL_XFER_REQUEST_TYPE_CLASS 0x20
#define U80211_DRV_KERNEL_XFER_REQUEST_TYPE_VENDOR 0x40
#define U80211_DRV_KERNEL_XFER_REQUEST_TYPE_RESERVED 0x60
#define U80211_DRV_KERNEL_XFER_REQUEST_TYPE_MASK 0x60

#define U80211_DRV_KERNEL_XFER_RECIPIENT_DEVICE 0x00
#define U80211_DRV_KERNEL_XFER_RECIPIENT_INTERFACE 0x01
#define U80211_DRV_KERNEL_XFER_RECIPIENT_ENDPOINT 0x02
#define U80211_DRV_KERNEL_XFER_RECIPIENT_OTHER 0x03
#define U80211_DRV_KERNEL_XFER_RECIPIENT_MASK 0x1f


#define U80211_DRV_KERNEL_PRINT_LEVEL_INFO 0
#define U80211_DRV_KERNEL_PRINT_LEVEL_WARN 1
#define U80211_DRV_KERNEL_PRINT_LEVEL_ERROR 2

void *u80211_drv_kernel_allocate(size_t size);
void u80211_drv_kernel_free(void *memory);
int u80211_drv_kernel_get_device_descriptor(u80211_drv_device_handle_t device, u80211_drv_device_descriptor_t *descriptor);
int u80211_drv_kernel_get_interface_descriptor(u80211_drv_interface_handle_t interface, u80211_drv_interface_descriptor_t *descriptor);
int u80211_drv_kernel_get_endpoints(u80211_drv_interface_handle_t interface, u80211_drv_endpoint_descriptor_t *endpoints, size_t endpoint_count);
int u80211_drv_kernel_submit_control_xfer_and_wait(u80211_drv_device_handle_t device, uint8_t flags, uint8_t request, uint16_t value, uint16_t index, void *buf, uint16_t buffer_size, size_t *transferred_size, unsigned int timeout);
int u80211_drv_kernel_submit_bulk_xfer_and_wait(u80211_drv_device_handle_t device, uint8_t endpoint_address, void *buf, size_t buffer_size, size_t *transferred_size, unsigned int timeout);
void u80211_drv_kernel_stall_us(unsigned int microseconds);
void u80211_drv_kernel_print(int level, const char *msg);

#endif
