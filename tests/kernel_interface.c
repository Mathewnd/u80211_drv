#include <stdio.h>
#include <stdlib.h>

#include <libusb.h>

#include <u80211_drv/status.h>
#include <u80211_drv/kernel_interface.h>

_Static_assert((U80211_DRV_KERNEL_XFER_OUT | U80211_DRV_KERNEL_XFER_REQUEST_TYPE_VENDOR | U80211_DRV_KERNEL_XFER_RECIPIENT_DEVICE) == 0x40, "invalid host-to-device vendor device flags");
_Static_assert((U80211_DRV_KERNEL_XFER_IN | U80211_DRV_KERNEL_XFER_REQUEST_TYPE_STANDARD | U80211_DRV_KERNEL_XFER_RECIPIENT_DEVICE) == 0x80, "invalid device-to-host standard device flags");
_Static_assert((U80211_DRV_KERNEL_XFER_OUT | U80211_DRV_KERNEL_XFER_REQUEST_TYPE_CLASS | U80211_DRV_KERNEL_XFER_RECIPIENT_INTERFACE) == 0x21, "invalid host-to-device class interface flags");
_Static_assert((U80211_DRV_KERNEL_XFER_IN | U80211_DRV_KERNEL_XFER_REQUEST_TYPE_VENDOR | U80211_DRV_KERNEL_XFER_RECIPIENT_ENDPOINT) == 0xc2, "invalid device-to-host vendor endpoint flags");

void *u80211_drv_kernel_allocate(size_t size) {
	return malloc(size);
}

void u80211_drv_kernel_free(void *memory) {
	free(memory);
}

int u80211_drv_kernel_get_device_descriptor(u80211_drv_device_handle_t device, u80211_drv_device_descriptor_t *descriptor) {
	struct libusb_device_descriptor usb_descriptor;
	if (libusb_get_device_descriptor(libusb_get_device(device), &usb_descriptor) != LIBUSB_SUCCESS)
		return U80211_DRV_STATUS_UNKNOWN_ERROR;

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
		return U80211_DRV_STATUS_UNKNOWN_ERROR;

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

int u80211_drv_kernel_submit_control_xfer_and_wait(u80211_drv_device_handle_t device, uint8_t flags, uint8_t request, uint16_t value, uint16_t index, void *buf, uint16_t buffer_size, unsigned int timeout) {
	return libusb_control_transfer(device, flags, request, value, index, buf, buffer_size, timeout);
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
