#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <libusb.h>

#include <u80211_drv/status.h>
#include <u80211_drv/u80211_drv.h>

enum {
	TEST_FAILURE = 1,
	TEST_SKIP = 77,
};

static int attach_tap(const char *name) {
	int descriptor = open("/dev/net/tun", O_RDWR | O_CLOEXEC);
	if (descriptor < 0) {
		fprintf(stderr, "SKIP: cannot open /dev/net/tun: %s\n", strerror(errno));
		return -1;
	}

	struct ifreq request = {0};
	request.ifr_flags = IFF_TAP | IFF_NO_PI;
	if (snprintf(request.ifr_name, sizeof(request.ifr_name), "%s", name)
			>= (int)sizeof(request.ifr_name)) {
		fprintf(stderr, "tap interface name is too long: %s\n", name);
		close(descriptor);
		errno = EINVAL;
		return -2;
	}

	if (ioctl(descriptor, TUNSETIFF, &request) < 0) {
		fprintf(stderr, "SKIP: cannot attach to %s: %s\n", name, strerror(errno));
		close(descriptor);
		return -1;
	}

	return descriptor;
}

int main(void) {
	libusb_context *usb_context = NULL;
	int usb_status = libusb_init(&usb_context);
	if (usb_status != LIBUSB_SUCCESS) {
		fprintf(stderr, "SKIP: libusb initialization failed: %s\n",
			libusb_error_name(usb_status));
		return TEST_SKIP;
	}

	libusb_device **devices = NULL;
	ssize_t device_count = libusb_get_device_list(usb_context, &devices);
	if (device_count < 0) {
		fprintf(stderr, "SKIP: USB device enumeration failed: %s\n",
			libusb_error_name((int)device_count));
		libusb_exit(usb_context);
		return TEST_SKIP;
	}

	libusb_device_handle *usb_handle = NULL;
	libusb_device_handle *matched_device = NULL;
	const struct libusb_interface_descriptor *matched_interface = NULL;
	struct libusb_config_descriptor *matched_config = NULL;
	u80211_drv_endpoint_handle_t *matched_endpoints = NULL;
	for (ssize_t i = 0; i < device_count; ++i) {
		libusb_device_handle *candidate_handle = NULL;
		usb_status = libusb_open(devices[i], &candidate_handle);
		if (usb_status != LIBUSB_SUCCESS)
			continue;

		struct libusb_config_descriptor *config = NULL;
		usb_status = libusb_get_config_descriptor(devices[i], 0, &config);
		if (usb_status != LIBUSB_SUCCESS) {
			fprintf(stderr, "USB configuration descriptor retrieval failed: %s\n",
				libusb_error_name(usb_status));
			libusb_close(candidate_handle);
			continue;
		}

		int matched = 0;
		for (uint8_t j = 0; j < config->bNumInterfaces && !matched; ++j) {
			for (int k = 0; k < config->interface[j].num_altsetting; ++k) {
				const struct libusb_interface_descriptor *candidate_interface =
					&config->interface[j].altsetting[k];
				if (u80211_drv_probe(candidate_handle, (void *)candidate_interface) == U80211_DRV_STATUS_SUCCESS) {
					matched_device = candidate_handle;
					matched_interface = candidate_interface;
					matched = 1;
					break;
				}
			}
		}
		if (!matched) {
			libusb_free_config_descriptor(config);
			libusb_close(candidate_handle);
			continue;
		}

		matched_config = config;
		usb_handle = matched_device;
		break;
	}

	if (usb_handle == NULL) {
		fprintf(stderr, "SKIP: no supported USB device found\n");
		libusb_free_device_list(devices, 1);
		libusb_exit(usb_context);
		return TEST_SKIP;
	}

	u80211_drv_interface_descriptor_t interface_descriptor;
	if (u80211_drv_kernel_get_interface_descriptor((void *)matched_interface, &interface_descriptor) != U80211_DRV_STATUS_SUCCESS) {
		libusb_close(usb_handle);
		libusb_free_config_descriptor(matched_config);
		libusb_free_device_list(devices, 1);
		libusb_exit(usb_context);
		return TEST_FAILURE;
	}

	if (interface_descriptor.endpoint_count != 0) {
		matched_endpoints = u80211_drv_kernel_allocate(interface_descriptor.endpoint_count * sizeof(*matched_endpoints));
		if (matched_endpoints == NULL) {
			libusb_close(usb_handle);
			libusb_free_config_descriptor(matched_config);
			libusb_free_device_list(devices, 1);
			libusb_exit(usb_context);
			return TEST_FAILURE;
		}
	}
	if (u80211_drv_kernel_get_endpoints((void *)matched_interface, matched_endpoints, interface_descriptor.endpoint_count) != U80211_DRV_STATUS_SUCCESS) {
		u80211_drv_kernel_free(matched_endpoints);
		libusb_close(usb_handle);
		libusb_free_config_descriptor(matched_config);
		libusb_free_device_list(devices, 1);
		libusb_exit(usb_context);
		return TEST_FAILURE;
	}
	for (size_t i = 0; i < interface_descriptor.endpoint_count; ++i) {
		u80211_drv_endpoint_descriptor_t endpoint_descriptor;
		if (u80211_drv_kernel_get_endpoint_descriptor(matched_endpoints[i], &endpoint_descriptor) != U80211_DRV_STATUS_SUCCESS) {
			for (size_t j = 0; j < i; ++j)
				u80211_drv_kernel_release_endpoint(matched_endpoints[j]);
			u80211_drv_kernel_free(matched_endpoints);
			libusb_close(usb_handle);
			libusb_free_config_descriptor(matched_config);
			libusb_free_device_list(devices, 1);
			libusb_exit(usb_context);
			return TEST_FAILURE;
		}
	}

	int tap_descriptor = attach_tap("tap0");
	if (tap_descriptor < 0) {
		for (size_t i = 0; i < interface_descriptor.endpoint_count; ++i)
			u80211_drv_kernel_release_endpoint(matched_endpoints[i]);
		u80211_drv_kernel_free(matched_endpoints);
		libusb_close(usb_handle);
		libusb_free_config_descriptor(matched_config);
		libusb_free_device_list(devices, 1);
		libusb_exit(usb_context);
		return tap_descriptor == -2 ? TEST_FAILURE : TEST_SKIP;
	}

	int attach_status = u80211_drv_attach(matched_device, (void *)matched_interface);
	if (attach_status != U80211_DRV_STATUS_SUCCESS) {
		fprintf(stderr, "driver attach failed for USB device: %d\n", attach_status);
		close(tap_descriptor);
		for (size_t i = 0; i < interface_descriptor.endpoint_count; ++i)
			u80211_drv_kernel_release_endpoint(matched_endpoints[i]);
		u80211_drv_kernel_free(matched_endpoints);
		libusb_close(usb_handle);
		libusb_free_config_descriptor(matched_config);
		libusb_free_device_list(devices, 1);
		libusb_exit(usb_context);
		return TEST_FAILURE;
	}

	printf("u80211_drv: attached USB device to tap0\n");

	close(tap_descriptor);
	for (size_t i = 0; i < interface_descriptor.endpoint_count; ++i)
		u80211_drv_kernel_release_endpoint(matched_endpoints[i]);
	u80211_drv_kernel_free(matched_endpoints);
	libusb_close(usb_handle);
	libusb_free_config_descriptor(matched_config);
	libusb_free_device_list(devices, 1);
	libusb_exit(usb_context);
	return 0;
}
