#include <stdio.h>

#include <libusb.h>

#include <u80211_drv/status.h>
#include <u80211_drv/kernel_interface.h>

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
	if (usb_descriptor == NULL)
		return U80211_DRV_STATUS_NO_MATCH;

	descriptor->number = usb_descriptor->bInterfaceNumber;
	descriptor->class_code = usb_descriptor->bInterfaceClass;
	descriptor->subclass = usb_descriptor->bInterfaceSubClass;
	descriptor->protocol = usb_descriptor->bInterfaceProtocol;
	return U80211_DRV_STATUS_SUCCESS;
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
