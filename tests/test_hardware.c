#include <stdbool.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libusb.h>

#include <u80211/status.h>
#include <u80211/u80211.h>
#include <u80211_drv/status.h>
#include <u80211_drv/u80211_drv.h>

enum {
	TEST_FAILURE = 1,
	TEST_SKIP = 77,
};

static bool ready_callback_called;
static int ready_callback_status = U80211_DRV_STATUS_UNKNOWN_ERROR;
static int scan_tx_status = U80211_STATUS_SUCCESS;

typedef struct {
	void *device;
	const u80211_drv_device_ops_t *ops;
} test_device_t;

typedef struct {
	libusb_context *context;
	pthread_t thread;
	bool stopping;
} usb_event_thread_t;

static void *usb_event_loop(void *context) {
	usb_event_thread_t *event_thread = context;
	while (!__atomic_load_n(&event_thread->stopping, __ATOMIC_ACQUIRE)) {
		int status = libusb_handle_events(event_thread->context);
		if (status != LIBUSB_SUCCESS && status != LIBUSB_ERROR_INTERRUPTED)
			break;
	}
	return NULL;
}

static int start_usb_event_thread(usb_event_thread_t *event_thread, libusb_context *context) {
	event_thread->context = context;
	__atomic_store_n(&event_thread->stopping, false, __ATOMIC_RELAXED);
	return pthread_create(&event_thread->thread, NULL, usb_event_loop, event_thread) == 0 ? U80211_DRV_STATUS_SUCCESS : U80211_DRV_STATUS_UNKNOWN_ERROR;
}

static void stop_usb_event_thread(usb_event_thread_t *event_thread) {
	__atomic_store_n(&event_thread->stopping, true, __ATOMIC_RELEASE);
	libusb_interrupt_event_handler(event_thread->context);
	pthread_join(event_thread->thread, NULL);
}

static int status_to_u80211(int status) {
	if (status == U80211_DRV_STATUS_SUCCESS)
		return U80211_STATUS_SUCCESS;
	if (status == U80211_DRV_STATUS_NOT_SUPPORTED)
		return U80211_STATUS_UNSUPPORTED;
	if (status == U80211_DRV_STATUS_INVALID_ARGUMENT)
		return U80211_STATUS_NOT_PERMITTED;
	if (status == U80211_DRV_STATUS_MALFORMED_PACKET)
		return U80211_STATUS_NOT_PERMITTED;
	if (status == U80211_DRV_STATUS_OUT_OF_MEMORY)
		return U80211_STATUS_ENOMEM;
	if (status == U80211_DRV_STATUS_TIMEOUT)
		return U80211_STATUS_TIMED_OUT;
	return U80211_STATUS_UNKNOWN_ERROR;
}

static int allocate_tx_buffer(u80211_device_t *device, size_t size, u80211_tx_buffer_descriptor_t *descriptor) {
	test_device_t *test_device = device->driver_data;
	void *buffer;
	int status = test_device->ops->allocate_tx_buffer(size, &buffer);
	if (status != U80211_DRV_STATUS_SUCCESS)
		return status_to_u80211(status);

	descriptor->data = buffer;
	descriptor->size = size;
	descriptor->current_offset = size;
	return U80211_STATUS_SUCCESS;
}

static int free_tx_buffer(u80211_device_t *device, u80211_tx_buffer_descriptor_t *descriptor) {
	test_device_t *test_device = device->driver_data;
	test_device->ops->free_tx_buffer(descriptor->data);

	descriptor->data = NULL;
	descriptor->size = 0;
	descriptor->current_offset = 0;
	return U80211_STATUS_SUCCESS;
}

static int transmit(u80211_device_t *device, u80211_tx_buffer_descriptor_t *descriptor) {
	test_device_t *test_device = device->driver_data;
	int status = test_device->ops->transmit(test_device->device, descriptor->data, descriptor->size, descriptor->current_offset);

	descriptor->data = NULL;
	descriptor->size = 0;
	descriptor->current_offset = 0;
	int u80211_status = status_to_u80211(status);
	if (u80211_status != U80211_STATUS_SUCCESS)
		__atomic_store_n(&scan_tx_status, u80211_status, __ATOMIC_RELEASE);
	return u80211_status;
}

static int set_channel(u80211_device_t *device, int channel) {
	if (channel < 1 || channel > UINT8_MAX)
		return U80211_STATUS_NOT_PERMITTED;

	test_device_t *test_device = device->driver_data;
	return status_to_u80211(test_device->ops->set_channel(test_device->device, (uint8_t)channel));
}

static const u80211_device_ops_t device_ops = {
	.allocate_tx_buffer = allocate_tx_buffer,
	.free_tx_buffer = free_tx_buffer,
	.transmit = transmit,
	.set_channel = set_channel,
};

static int print_scan_results(u80211_device_t *device) {
	size_t cache_count = u80211_bss_cache_get_count(&device->bss_cache);
	printf("scan complete: %zu access point%s\n", cache_count, cache_count == 1 ? "" : "s");
	if (cache_count == 0)
		return U80211_STATUS_SUCCESS;

	u80211_ap_t **aps = malloc(cache_count * sizeof(*aps));
	if (aps == NULL)
		return U80211_STATUS_ENOMEM;

	size_t ap_count = u80211_bss_cache_get_aps(&device->bss_cache, aps, cache_count);
	for (size_t i = 0; i < ap_count; ++i) {
		u80211_ap_t *ap = aps[i];
		printf(
			"SSID=\"%s\" channel=%u BSSID=%02x:%02x:%02x:%02x:%02x:%02x interval=%u capabilities=0x%04x rates=",
			ap->ssid,
			(unsigned int)ap->channel,
			ap->mac_address.bytes[0], ap->mac_address.bytes[1], ap->mac_address.bytes[2],
			ap->mac_address.bytes[3], ap->mac_address.bytes[4], ap->mac_address.bytes[5],
			(unsigned int)ap->interval,
			(unsigned int)ap->capabilities
		);
		for (size_t rate = 0; rate < sizeof(ap->rate_bitmap); ++rate)
			printf("%s%02x", rate == 0 ? "" : ":", ap->rate_bitmap[rate]);
		putchar('\n');
		u80211_ap_release(ap);
	}

	free(aps);
	return ap_count == cache_count ? U80211_STATUS_SUCCESS : U80211_STATUS_UNKNOWN_ERROR;
}

int u80211_drv_device_ready(void *driver_device, const u80211_drv_device_metadata_t *driver_metadata, const u80211_drv_device_ops_t *driver_ops, u80211_drv_network_device_handle_t *network_device) {
	ready_callback_called = true;
	__atomic_store_n(&scan_tx_status, U80211_STATUS_SUCCESS, __ATOMIC_RELAXED);
	test_device_t test_device = {
		.device = driver_device,
		.ops = driver_ops,
	};

	u80211_device_metadata_t metadata = {0};
	memcpy(metadata.mac_address.bytes, driver_metadata->mac_address, sizeof(metadata.mac_address.bytes));
	memcpy(metadata.rate_bitmap, driver_metadata->rate_bitmap, sizeof(metadata.rate_bitmap));

	u80211_device_t *device = NULL;
	int status = u80211_register_device(&metadata, &device_ops, &test_device, &device);
	if (status != U80211_STATUS_SUCCESS)
		goto done;
	*network_device = device;

	status = u80211_scan(device);
	if (status != U80211_STATUS_SUCCESS)
		goto unregister;
	status = u80211_wait_for_scan_completion(device);
	if (status != U80211_STATUS_SUCCESS)
		goto unregister;
	status = __atomic_load_n(&scan_tx_status, __ATOMIC_ACQUIRE);
	if (status != U80211_STATUS_SUCCESS) {
		fprintf(stderr, "scan probe transmission failed: %d\n", status);
		goto unregister;
	}
	status = print_scan_results(device);

unregister:
	*network_device = NULL;
	u80211_unregister_device(device);
done:
	ready_callback_status = status == U80211_STATUS_SUCCESS
		? U80211_DRV_STATUS_SUCCESS
		: U80211_DRV_STATUS_UNKNOWN_ERROR;
	return ready_callback_status;
}

int main(void) {
	libusb_context *usb_context = NULL;
	int usb_status = libusb_init(&usb_context);
	if (usb_status != LIBUSB_SUCCESS) {
		fprintf(stderr, "SKIP: libusb initialization failed: %s\n", libusb_error_name(usb_status));
		return TEST_SKIP;
	}

	libusb_device **devices = NULL;
	ssize_t device_count = libusb_get_device_list(usb_context, &devices);
	if (device_count < 0) {
		fprintf(stderr, "SKIP: USB device enumeration failed: %s\n", libusb_error_name((int)device_count));
		libusb_exit(usb_context);
		return TEST_SKIP;
	}

	libusb_device_handle *matched_device = NULL;
	const struct libusb_interface_descriptor *matched_interface = NULL;
	struct libusb_config_descriptor *matched_config = NULL;
	for (ssize_t i = 0; i < device_count; ++i) {
		libusb_device_handle *candidate_handle = NULL;
		usb_status = libusb_open(devices[i], &candidate_handle);
		if (usb_status != LIBUSB_SUCCESS)
			continue;

		struct libusb_config_descriptor *config = NULL;
		usb_status = libusb_get_config_descriptor(devices[i], 0, &config);
		if (usb_status != LIBUSB_SUCCESS) {
			fprintf(stderr, "USB configuration descriptor retrieval failed: %s\n", libusb_error_name(usb_status));
			libusb_close(candidate_handle);
			continue;
		}

		bool matched = false;
		for (uint8_t j = 0; j < config->bNumInterfaces && !matched; ++j) {
			for (int k = 0; k < config->interface[j].num_altsetting; ++k) {
				const struct libusb_interface_descriptor *candidate_interface = &config->interface[j].altsetting[k];
				if (u80211_drv_probe(candidate_handle, (void *)candidate_interface) == U80211_DRV_STATUS_SUCCESS) {
					matched_device = candidate_handle;
					matched_interface = candidate_interface;
					matched = true;
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
		break;
	}

	if (matched_device == NULL) {
		fprintf(stderr, "SKIP: no supported USB device found\n");
		libusb_free_device_list(devices, 1);
		libusb_exit(usb_context);
		return TEST_SKIP;
	}

	usb_status = libusb_reset_device(matched_device);
	if (usb_status != LIBUSB_SUCCESS) {
		fprintf(stderr, "USB device reset failed: %s\n", libusb_error_name(usb_status));
		libusb_close(matched_device);
		libusb_free_config_descriptor(matched_config);
		libusb_free_device_list(devices, 1);
		libusb_exit(usb_context);
		return TEST_FAILURE;
	}

	libusb_set_auto_detach_kernel_driver(matched_device, 1);
	usb_status = libusb_claim_interface(matched_device, matched_interface->bInterfaceNumber);
	if (usb_status != LIBUSB_SUCCESS) {
		fprintf(stderr, "USB interface claim failed: %s\n", libusb_error_name(usb_status));
		libusb_close(matched_device);
		libusb_free_config_descriptor(matched_config);
		libusb_free_device_list(devices, 1);
		libusb_exit(usb_context);
		return TEST_FAILURE;
	}

	usb_event_thread_t event_thread;
	if (start_usb_event_thread(&event_thread, usb_context) != U80211_DRV_STATUS_SUCCESS) {
		fprintf(stderr, "USB event thread initialization failed\n");
		libusb_release_interface(matched_device, matched_interface->bInterfaceNumber);
		libusb_close(matched_device);
		libusb_free_config_descriptor(matched_config);
		libusb_free_device_list(devices, 1);
		libusb_exit(usb_context);
		return TEST_FAILURE;
	}

	int attach_status = u80211_drv_attach(matched_device, (void *)matched_interface);
	int result = TEST_FAILURE;
	if (attach_status != U80211_DRV_STATUS_SUCCESS)
		fprintf(stderr, "driver attach failed for USB device: %d\n", attach_status);
	else if (!ready_callback_called)
		fprintf(stderr, "driver attach completed without a device-ready callback\n");
	else if (ready_callback_status != U80211_DRV_STATUS_SUCCESS)
		fprintf(stderr, "device-ready callback failed: %d\n", ready_callback_status);
	else {
		puts("u80211_drv: hardware scan scaffold completed");
		result = 0;
	}

	stop_usb_event_thread(&event_thread);
	libusb_release_interface(matched_device, matched_interface->bInterfaceNumber);
	libusb_close(matched_device);
	libusb_free_config_descriptor(matched_config);
	libusb_free_device_list(devices, 1);
	libusb_exit(usb_context);
	return result;
}
