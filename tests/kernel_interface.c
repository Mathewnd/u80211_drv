#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <libusb.h>

#include <u80211_drv/status.h>
#include <u80211_drv/kernel_interface.h>

_Static_assert((U80211_DRV_KERNEL_XFER_OUT | U80211_DRV_KERNEL_XFER_REQUEST_TYPE_VENDOR | U80211_DRV_KERNEL_XFER_RECIPIENT_DEVICE) == 0x40, "invalid host-to-device vendor device flags");
_Static_assert((U80211_DRV_KERNEL_XFER_IN | U80211_DRV_KERNEL_XFER_REQUEST_TYPE_STANDARD | U80211_DRV_KERNEL_XFER_RECIPIENT_DEVICE) == 0x80, "invalid device-to-host standard device flags");
_Static_assert((U80211_DRV_KERNEL_XFER_OUT | U80211_DRV_KERNEL_XFER_REQUEST_TYPE_CLASS | U80211_DRV_KERNEL_XFER_RECIPIENT_INTERFACE) == 0x21, "invalid host-to-device class interface flags");
_Static_assert((U80211_DRV_KERNEL_XFER_IN | U80211_DRV_KERNEL_XFER_REQUEST_TYPE_VENDOR | U80211_DRV_KERNEL_XFER_RECIPIENT_ENDPOINT) == 0xc2, "invalid device-to-host vendor endpoint flags");

static int u80211_drv_kernel_status_from_libusb(int status) {
	if (status == LIBUSB_SUCCESS)
		return U80211_DRV_STATUS_SUCCESS;
	if (status == LIBUSB_ERROR_TIMEOUT)
		return U80211_DRV_STATUS_TIMEOUT;
	return U80211_DRV_STATUS_UNKNOWN_ERROR;
}

void *u80211_drv_kernel_allocate(size_t size) {
	return malloc(size);
}

void u80211_drv_kernel_free(void *memory) {
	free(memory);
}

int u80211_drv_kernel_get_device_descriptor(u80211_drv_device_handle_t device, u80211_drv_device_descriptor_t *descriptor) {
	struct libusb_device_descriptor usb_descriptor;
	int status = libusb_get_device_descriptor(libusb_get_device(device), &usb_descriptor);
	if (status != LIBUSB_SUCCESS)
		return u80211_drv_kernel_status_from_libusb(status);

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

int u80211_drv_kernel_get_endpoints(u80211_drv_interface_handle_t interface, u80211_drv_endpoint_descriptor_t *endpoints, size_t endpoint_count) {
	const struct libusb_interface_descriptor *usb_descriptor = interface;
	if (usb_descriptor == NULL || endpoint_count != usb_descriptor->bNumEndpoints || (endpoint_count != 0 && endpoints == NULL))
		return U80211_DRV_STATUS_UNKNOWN_ERROR;

	for (size_t i = 0; i < endpoint_count; ++i) {
		endpoints[i].address = usb_descriptor->endpoint[i].bEndpointAddress;
		endpoints[i].attributes = usb_descriptor->endpoint[i].bmAttributes;
		endpoints[i].maximum_packet_size = usb_descriptor->endpoint[i].wMaxPacketSize;
		endpoints[i].interval = usb_descriptor->endpoint[i].bInterval;
	}

	return U80211_DRV_STATUS_SUCCESS;
}

int u80211_drv_kernel_submit_control_xfer_and_wait(u80211_drv_device_handle_t device, uint8_t flags, uint8_t request, uint16_t value, uint16_t index, void *buf, uint16_t buffer_size, size_t *transferred_size, unsigned int timeout) {
	int result = libusb_control_transfer(device, flags, request, value, index, buf, buffer_size, timeout);
	if (result < 0)
		return u80211_drv_kernel_status_from_libusb(result);

	*transferred_size = result;
	return U80211_DRV_STATUS_SUCCESS;
}

int u80211_drv_kernel_submit_bulk_xfer_and_wait(u80211_drv_device_handle_t device, uint8_t endpoint_address, void *buf, size_t buffer_size, size_t *transferred_size, unsigned int timeout) {
	int transferred = 0;
	int status = libusb_bulk_transfer(device, endpoint_address, buf, buffer_size, &transferred, timeout);
	if (status != LIBUSB_SUCCESS)
		return u80211_drv_kernel_status_from_libusb(status);

	*transferred_size = transferred;
	return U80211_DRV_STATUS_SUCCESS;
}

void u80211_drv_kernel_stall_us(unsigned int microseconds) {
	struct timespec remaining = {
		.tv_sec = microseconds / 1000000,
		.tv_nsec = (long)(microseconds % 1000000) * 1000,
	};

	while (nanosleep(&remaining, &remaining) < 0 && errno == EINTR)
		;
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
