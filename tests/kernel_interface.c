#include <stdio.h>
#include <stdlib.h>

#include <libusb.h>

#include <u80211_drv/status.h>
#include <u80211_drv/kernel_interface.h>

void *u80211_drv_kernel_allocate(size_t size) {
	return malloc(size);
}

void u80211_drv_kernel_free(void *memory) {
	free(memory);
}

int u80211_drv_kernel_get_device_descriptor(u80211_drv_device_handle_t device, u80211_drv_device_descriptor_t *descriptor) {
	struct libusb_device_descriptor usb_descriptor;
	if (libusb_get_device_descriptor(device, &usb_descriptor) != LIBUSB_SUCCESS)
		return U80211_DRV_STATUS_NO_MATCH;

	descriptor->vendor_id = usb_descriptor.idVendor;
	descriptor->product_id = usb_descriptor.idProduct;
	return U80211_DRV_STATUS_SUCCESS;
}

int u80211_drv_kernel_get_interface_descriptor(u80211_drv_interface_handle_t interface, u80211_drv_interface_descriptor_t *descriptor) {
	const struct libusb_interface_descriptor *usb_descriptor = interface;

	descriptor->number = usb_descriptor->bInterfaceNumber;
	descriptor->class_code = usb_descriptor->bInterfaceClass;
	descriptor->subclass = usb_descriptor->bInterfaceSubClass;
	descriptor->protocol = usb_descriptor->bInterfaceProtocol;
	descriptor->endpoint_count = usb_descriptor->bNumEndpoints;
	return U80211_DRV_STATUS_SUCCESS;
}

int u80211_drv_kernel_get_endpoints(u80211_drv_interface_handle_t interface, u80211_drv_endpoint_handle_t *endpoints, size_t endpoint_count) {
	const struct libusb_interface_descriptor *usb_descriptor = interface;
	if (usb_descriptor == NULL || endpoint_count != usb_descriptor->bNumEndpoints || (endpoint_count != 0 && endpoints == NULL))
		return U80211_DRV_STATUS_NO_MATCH;

	for (size_t i = 0; i < endpoint_count; ++i)
		endpoints[i] = (void *)&usb_descriptor->endpoint[i];

	return U80211_DRV_STATUS_SUCCESS;
}

int u80211_drv_kernel_get_endpoint_descriptor(u80211_drv_endpoint_handle_t endpoint, u80211_drv_endpoint_descriptor_t *descriptor) {
	const struct libusb_endpoint_descriptor *usb_descriptor = endpoint;

	descriptor->address = usb_descriptor->bEndpointAddress;
	descriptor->attributes = usb_descriptor->bmAttributes;
	descriptor->maximum_packet_size = usb_descriptor->wMaxPacketSize;
	descriptor->interval = usb_descriptor->bInterval;
	return U80211_DRV_STATUS_SUCCESS;
}

void u80211_drv_kernel_release_endpoint(u80211_drv_endpoint_handle_t endpoint) {
	(void)endpoint;
}

#define U80211_DRV_KERNEL_PRINT_LEVEL_INFO 0
#define U80211_DRV_KERNEL_PRINT_LEVEL_WARN 1
#define U80211_DRV_KERNEL_PRINT_LEVEL_ERROR 2
void u80211_drv_kernel_print(int level, const char *msg) {
	switch (level) {
		case U80211_DRV_KERNEL_PRINT_LEVEL_INFO:
			fprintf(stderr, "[INFO] %s\n", msg);
			break;
		case U80211_DRV_KERNEL_PRINT_LEVEL_WARN:
			fprintf(stderr, "[WARN] %s\n", msg);
			break;
		case U80211_DRV_KERNEL_PRINT_LEVEL_ERROR:
			fprintf(stderr, "[ERROR] %s\n", msg);
			break;
		default:
			fprintf(stderr, "[UNKNOWN] %s\n", msg);
			break;
	}
}
