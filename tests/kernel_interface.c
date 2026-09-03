#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include <libusb.h>

#include <u80211_drv/status.h>
#include <u80211_drv/kernel_interface.h>

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

int u80211_drv_kernel_get_firmware(const char *name, u80211_drv_kernel_firmware_callback_t callback, void *context) {
	int firmware_directory = open("firmware_blobs", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
	if (firmware_directory < 0)
		return U80211_DRV_STATUS_UNKNOWN_ERROR;

	int firmware = openat(firmware_directory, name, O_RDONLY | O_CLOEXEC);
	close(firmware_directory);
	if (firmware < 0)
		return U80211_DRV_STATUS_UNKNOWN_ERROR;

	struct stat firmware_stat;
	if (fstat(firmware, &firmware_stat) != 0) {
		close(firmware);
		return U80211_DRV_STATUS_UNKNOWN_ERROR;
	}

	size_t firmware_size = (size_t)firmware_stat.st_size;
	void *firmware_data = malloc(firmware_size);
	if (firmware_data == NULL) {
		close(firmware);
		return U80211_DRV_STATUS_OUT_OF_MEMORY;
	}

	ssize_t read_size = read(firmware, firmware_data, firmware_size);
	if ((size_t)read_size != firmware_size) {
		free(firmware_data);
		close(firmware);
		return U80211_DRV_STATUS_UNKNOWN_ERROR;
	}

	close(firmware);
	callback(context, firmware_data, firmware_size);
	free(firmware_data);
	return U80211_DRV_STATUS_SUCCESS;
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
