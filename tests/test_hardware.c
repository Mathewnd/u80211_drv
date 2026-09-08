#define _GNU_SOURCE

#include <stdbool.h>
#include <errno.h>
#include <fcntl.h>
#include <net/if.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include <linux/if_tun.h>

#include <libusb.h>

#include <u80211/status.h>
#include <u80211/u80211.h>
#include <u80211_drv/status.h>
#include <u80211_drv/u80211_drv.h>

#include "wpas_server.h"
#include "wpas/wpas_protocol.h"

enum {
	TEST_FAILURE = 1,
	TEST_SKIP = 77,
};

static bool ready_callback_called;
static int ready_callback_status = U80211_DRV_STATUS_UNKNOWN_ERROR;
static pthread_t usb_event_thread;

typedef struct {
	void *device;
	const u80211_drv_device_ops_t *ops;
	pthread_mutex_t receive_mutex;
	u80211_device_t *u80211_device;
	int tap_fd;
	pthread_t tap_thread;
	bool tap_thread_started;
	bool dhcp_started;
	u80211_wpas_server_t *wpas_server;
} test_device_t;

static bool trace_packets(void) {
	const char *value = getenv("U80211_TRACE_PACKETS");
	return value != NULL && value[0] != '\0' && strcmp(value, "0") != 0;
}

static void *tap_io_loop(void *context) {
	test_device_t *test_device = context;
	uint8_t frame[1514];
	for (;;) {
		ssize_t size = read(test_device->tap_fd, frame, sizeof(frame));
		if (size < 0) {
			if (errno == EINTR)
				continue;
			break;
		}
		if (size == 0)
			continue;

		u80211_device_t *device = test_device->u80211_device;
		if (device == NULL)
			continue;
		u80211_tx_buffer_descriptor_t descriptor;
		if (u80211_allocate_tx_buffer(device, &descriptor) != U80211_STATUS_SUCCESS)
			continue;
		if ((size_t)size > descriptor.size) {
			device->ops->free_tx_buffer(device, &descriptor);
			continue;
		}
		descriptor.current_offset = descriptor.size - (size_t)size;
		memcpy((uint8_t *)descriptor.data + descriptor.current_offset, frame, (size_t)size);
		int status = u80211_transmit_buffer(device, &descriptor);
		if (trace_packets()) {
			uint16_t ethertype = size >= 14 ? ((uint16_t)frame[12] << 8) | frame[13] : 0;
			fprintf(stderr, "u80211 trace: TAP TX size=%zd ethertype=0x%04x status=%d\n", size, ethertype, status);
		}
	}
	return NULL;
}

static void *usb_event_loop(void *context) {
	libusb_context *usb_context = context;
	for (;;)
		libusb_handle_events(usb_context);
	return NULL;
}

static int start_usb_event_thread(libusb_context *context) {
	return pthread_create(&usb_event_thread, NULL, usb_event_loop, context) == 0 ? U80211_DRV_STATUS_SUCCESS : U80211_DRV_STATUS_UNKNOWN_ERROR;
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

static int transmit(u80211_device_t *device, u80211_tx_buffer_descriptor_t *descriptor, const u80211_transmit_options_t *options) {
	test_device_t *test_device = device->driver_data;
	u80211_drv_transmit_options_t driver_options = {
		.key = options == NULL ? -1 : options->key,
		.cipher = U80211_DRV_CIPHER_NONE,
	};
	int status;
	if (options != NULL) {
		switch (options->cipher) {
			case -1:
				break;
			case U80211_CIPHER_CCMP:
				driver_options.cipher = U80211_DRV_CIPHER_CCMP;
				break;
			case U80211_CIPHER_TKIP:
				driver_options.cipher = U80211_DRV_CIPHER_TKIP;
				break;
			default:
				status = U80211_STATUS_UNSUPPORTED;
				goto free_buffer;
		}
	}

	status = status_to_u80211(test_device->ops->transmit(test_device->device, descriptor->data,
		descriptor->size, descriptor->current_offset, &driver_options));
	goto clear_descriptor;

free_buffer:
	test_device->ops->free_tx_buffer(descriptor->data);

clear_descriptor:
	descriptor->data = NULL;
	descriptor->size = 0;
	descriptor->current_offset = 0;
	return status;
}

static int set_channel(u80211_device_t *device, int channel) {
	if (channel < 1 || channel > UINT8_MAX)
		return U80211_STATUS_NOT_PERMITTED;

	test_device_t *test_device = device->driver_data;
	return status_to_u80211(test_device->ops->set_channel(test_device->device, (uint8_t)channel));
}

static int set_key(u80211_device_t *device, const u80211_key_t *key) {
	if (key == NULL)
		return U80211_STATUS_NOT_PERMITTED;

	u80211_drv_key_t driver_key = {
		.index = key->index,
		.key = key->key,
		.key_len = key->key_len,
	};
	if (key->cipher == U80211_CIPHER_CCMP)
		driver_key.cipher = U80211_DRV_CIPHER_CCMP;
	else if (key->cipher == U80211_CIPHER_TKIP)
		driver_key.cipher = U80211_DRV_CIPHER_TKIP;
	else
		return U80211_STATUS_UNSUPPORTED;

	memcpy(driver_key.peer, key->peer.bytes, sizeof(driver_key.peer));
	if (key->flags & U80211_KEY_PAIRWISE)
		driver_key.flags |= U80211_DRV_KEY_PAIRWISE;
	if (key->flags & U80211_KEY_GROUP)
		driver_key.flags |= U80211_DRV_KEY_GROUP;
	if (key->flags & U80211_KEY_RX)
		driver_key.flags |= U80211_DRV_KEY_RX;
	if (key->flags & U80211_KEY_TX)
		driver_key.flags |= U80211_DRV_KEY_TX;

	test_device_t *test_device = device->driver_data;
	return status_to_u80211(test_device->ops->set_key(test_device->device, &driver_key));
}

static int delete_key(u80211_device_t *device, uint8_t index, const u80211_mac_address_t *peer, uint32_t flags) {
	(void)peer;
	(void)flags;
	test_device_t *test_device = device->driver_data;
	return status_to_u80211(test_device->ops->del_key(test_device->device, index));
}

static const u80211_device_ops_t device_ops = {
	.allocate_tx_buffer = allocate_tx_buffer,
	.free_tx_buffer = free_tx_buffer,
	.transmit = transmit,
	.set_channel = set_channel,
	.set_key = set_key,
	.del_key = delete_key,
};

static void wait_for_association_work(void) {
	unsigned int seconds = 6;
	while (seconds != 0)
		seconds = sleep(seconds);
}

static bool configure_tap(u80211_device_t *device) {
	char mac[18];
	if (snprintf(mac, sizeof(mac), "%02x:%02x:%02x:%02x:%02x:%02x",
		device->metadata.mac_address.bytes[0], device->metadata.mac_address.bytes[1],
		device->metadata.mac_address.bytes[2], device->metadata.mac_address.bytes[3],
		device->metadata.mac_address.bytes[4], device->metadata.mac_address.bytes[5]) != 17)
		return false;

	char command[128];
	if (snprintf(command, sizeof(command), "ip link set dev tap0 down") < 0 || system(command) != 0)
		return false;
	if (snprintf(command, sizeof(command), "ip link set dev tap0 address %s", mac) < 0 || system(command) != 0)
		return false;
	if (snprintf(command, sizeof(command), "ip link set dev tap0 up") < 0 || system(command) != 0)
		return false;
	return true;
}

static bool in_private_network_namespace(void) {
	struct stat self_namespace;
	struct stat init_namespace;
	if (stat("/proc/self/ns/net", &self_namespace) != 0 ||
		stat("/proc/1/ns/net", &init_namespace) != 0)
		return false;
	return self_namespace.st_ino != init_namespace.st_ino ||
		self_namespace.st_dev != init_namespace.st_dev;
}

static int run_interactive_shell(void) {
	const char *shell = getenv("SHELL");
	if (shell == NULL || access(shell, X_OK) != 0)
		shell = "/bin/sh";

	pid_t child = fork();
	if (child < 0)
		return -1;
	if (child == 0) {
		execl(shell, shell, "-i", (char *)NULL);
		_exit(127);
	}

	int child_status;
	while (waitpid(child, &child_status, 0) < 0) {
		if (errno != EINTR)
			return -1;
	}
	return child_status;
}

static int start_dhcp(void) {
	pid_t child = fork();
	if (child < 0)
		return -1;
	if (child == 0) {
		execlp(
			"dhcpcd", "dhcpcd", "-4", "-B",
			"-C", "resolv.conf",
			"-C", "hostname",
			"-C", "timesyncd.conf",
			"tap0", (char *)NULL
		);
		_exit(127);
	}
	return 0;
}

static void association_ready(void *context) {
	test_device_t *test_device = context;
	if (__atomic_exchange_n(&test_device->dhcp_started, true, __ATOMIC_ACQ_REL))
		return;
	if (!in_private_network_namespace()) {
		fputs("skipping DHCP in the host network namespace; use run_with_ns.sh\n", stderr);
		return;
	}
	if (start_dhcp() != 0)
		fputs("could not launch isolated IPv4 DHCP on tap0\n", stderr);
}

static int open_tap(void) {
	int tap = open("/dev/net/tun", O_RDWR);
	if (tap < 0)
		return -1;
	if (fcntl(tap, F_SETFD, FD_CLOEXEC) < 0) {
		close(tap);
		return -1;
	}

	struct ifreq interface = {0};
	strncpy(interface.ifr_name, "tap0", sizeof(interface.ifr_name) - 1);
	interface.ifr_flags = IFF_TAP | IFF_NO_PI;
	if (ioctl(tap, TUNSETIFF, &interface) < 0) {
		close(tap);
		return -1;
	}

	return tap;
}

void u80211_drv_packet_received(u80211_drv_network_device_handle_t network_device, void *packet, size_t packet_size) {
	test_device_t *test_device = network_device;
	pthread_mutex_lock(&test_device->receive_mutex);
	u80211_device_t *device = test_device->u80211_device;
	if (device != NULL) {
		if (trace_packets()) {
			uint16_t frame_control = packet_size >= 2 ? (uint16_t)((uint8_t *)packet)[0] | ((uint16_t)((uint8_t *)packet)[1] << 8) : 0;
			fprintf(stderr, "u80211 trace: radio RX size=%zu frame_control=0x%04x\n", packet_size, frame_control);
		}
		u80211_process_packet(device, packet, packet_size);
	}
	pthread_mutex_unlock(&test_device->receive_mutex);
}

void u80211_kernel_receive_callback(u80211_device_t *device, void *buffer, size_t size) {
	test_device_t *test_device = device->driver_data;
	if (test_device == NULL || test_device->tap_fd < 0)
		return;
	if (trace_packets()) {
		const uint8_t *frame = buffer;
		uint16_t ethertype = size >= 14 ? ((uint16_t)frame[12] << 8) | frame[13] : 0;
		fprintf(stderr, "u80211 trace: TAP RX size=%zu ethertype=0x%04x\n", size, ethertype);
	}
	ssize_t written = write(test_device->tap_fd, buffer, size);
	(void)written;
}

int u80211_drv_device_ready(void *driver_device, const u80211_drv_device_metadata_t *driver_metadata, const u80211_drv_device_ops_t *driver_ops, u80211_drv_network_device_handle_t *network_device) {
	ready_callback_called = true;
	test_device_t *test_device = malloc(sizeof(*test_device));
	if (test_device == NULL) {
		ready_callback_status = U80211_DRV_STATUS_OUT_OF_MEMORY;
		return ready_callback_status;
	}
	*test_device = (test_device_t){
		.device = driver_device,
		.ops = driver_ops,
		.u80211_device = NULL,
		.tap_fd = -1,
	};
	if (pthread_mutex_init(&test_device->receive_mutex, NULL) != 0) {
		free(test_device);
		ready_callback_status = U80211_DRV_STATUS_UNKNOWN_ERROR;
		return ready_callback_status;
	}

	u80211_device_metadata_t metadata = {0};
	memcpy(metadata.mac_address.bytes, driver_metadata->mac_address, sizeof(metadata.mac_address.bytes));
	memcpy(metadata.rate_bitmap, driver_metadata->rate_bitmap, sizeof(metadata.rate_bitmap));

	u80211_device_t *device = NULL;
	int status = u80211_register_device(&metadata, &device_ops, test_device, &device);
	if (status != U80211_STATUS_SUCCESS) {
		pthread_mutex_destroy(&test_device->receive_mutex);
		free(test_device);
		goto done;
	}

	test_device->u80211_device = device;
	__atomic_store_n(network_device, test_device, __ATOMIC_RELEASE);

	int tap = open_tap();
	if (tap < 0) {
		fputs("could not open tap0 for packet I/O\n", stderr);
		status = U80211_STATUS_UNKNOWN_ERROR;
		goto unregister;
	}
	if (!configure_tap(device)) {
		fputs("could not configure tap0\n", stderr);
		close(tap);
		status = U80211_STATUS_UNKNOWN_ERROR;
		goto unregister;
	}
	test_device->tap_fd = tap;
	if (pthread_create(&test_device->tap_thread, NULL, tap_io_loop, test_device) != 0) {
		fputs("could not start tap packet I/O thread\n", stderr);
		status = U80211_STATUS_UNKNOWN_ERROR;
		goto unregister;
	}
	test_device->tap_thread_started = true;

	if (u80211_wpas_server_start(device, association_ready, test_device, &test_device->wpas_server) != 0) {
		fprintf(stderr, "could not start wpa_supplicant control socket %s: %s\n", U80211_WPAS_SOCKET_PATH, strerror(errno));
		status = U80211_STATUS_UNKNOWN_ERROR;
		goto unregister;
	}

	puts("u80211 hardware is ready; starting namespace shell");
	puts("run: ./wpa_install/sbin/wpa_supplicant -Du80211 -itap0 -c tests/wpa_supplicant.conf");
	fflush(stdout);
	if (run_interactive_shell() < 0) {
		fputs("could not start the network namespace shell\n", stderr);
		status = U80211_STATUS_UNKNOWN_ERROR;
	}

unregister:
	u80211_wpas_server_stop(test_device->wpas_server);
	test_device->wpas_server = NULL;
	int device_state = u80211_get_device_state(device);
	if (device_state == U80211_DEVICE_STATE_SCANNING)
		u80211_wait_for_scan_completion(device);
	else if (device_state == U80211_DEVICE_STATE_AUTHENTICATING || device_state == U80211_DEVICE_STATE_ASSOCIATING)
		u80211_wait_for_association_completion(device);
	if (u80211_get_device_state(device) == U80211_DEVICE_STATE_ASSOCIATED) {
		u80211_disassociate(device);
		wait_for_association_work();
	}
	if (test_device->tap_thread_started) {
		pthread_cancel(test_device->tap_thread);
		pthread_join(test_device->tap_thread, NULL);
		test_device->tap_thread_started = false;
	}
	if (test_device->tap_fd >= 0) {
		close(test_device->tap_fd);
		test_device->tap_fd = -1;
	}
	pthread_mutex_lock(&test_device->receive_mutex);
	test_device->u80211_device = NULL;
	pthread_mutex_unlock(&test_device->receive_mutex);
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

	if (start_usb_event_thread(usb_context) != U80211_DRV_STATUS_SUCCESS) {
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
		puts("u80211_drv: wpa_supplicant hardware session completed");
		result = 0;
	}

	return result;
}
