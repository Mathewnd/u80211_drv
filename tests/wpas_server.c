#define _GNU_SOURCE

#include "wpas_server.h"

#include <endian.h>
#include <errno.h>
#include <poll.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include <u80211/status.h>
#include <u80211/kernel_interface.h>
#include "wpas/wpas_protocol.h"

#define IEEE80211_CAPABILITY_PRIVACY 0x0010
#define SERVER_POLL_TIMEOUT_MS 100

struct u80211_wpas_server {
	u80211_device_t *device;
	int listen_fd;
	int client_fd;
	pthread_t thread;
	bool stopping;
	bool scan_pending;
	bool association_pending;
	int previous_state;
	u80211_wpas_associated_fn_t associated;
	void *associated_context;
};

static bool server_stopping(u80211_wpas_server_t *server) {
	return __atomic_load_n(&server->stopping, __ATOMIC_ACQUIRE);
}

static int send_packet(u80211_wpas_server_t *server, uint16_t type, uint32_t request_id, int status, const void *payload, uint32_t payload_length) {
	if (server->client_fd < 0)
		return -1;

	u80211_wpas_header_t header = {
		.magic = htole32(U80211_WPAS_PROTOCOL_MAGIC),
		.version = htole16(U80211_WPAS_PROTOCOL_VERSION),
		.type = htole16(type),
		.request_id = htole32(request_id),
		.payload_length = htole32(payload_length),
		.status = (int32_t)htole32((uint32_t)status),
	};
	struct iovec iov[2] = {
		{ .iov_base = &header, .iov_len = sizeof(header) },
		{ .iov_base = (void *)payload, .iov_len = payload_length },
	};
	struct msghdr message = {
		.msg_iov = iov,
		.msg_iovlen = payload_length == 0 ? 1 : 2,
	};
	ssize_t sent = sendmsg(server->client_fd, &message, MSG_NOSIGNAL);
	return sent == (ssize_t)(sizeof(header) + payload_length) ? 0 : -1;
}

static int send_response(u80211_wpas_server_t *server, uint32_t request_id, int status, const void *payload, uint32_t payload_length) {
	return send_packet(server, U80211_WPAS_RESPONSE, request_id, status, payload, payload_length);
}

static int send_event(u80211_wpas_server_t *server, uint16_t type, int status, const void *payload, uint32_t payload_length) {
	return send_packet(server, type, 0, status, payload, payload_length);
}

static int link_snapshot(u80211_wpas_server_t *server, u80211_wpas_link_t *link) {
	memset(link, 0, sizeof(*link));
	u80211_kernel_acquire_spinlock(server->device->association_spinlock);
	int state = u80211_get_device_state(server->device);
	if (state == U80211_DEVICE_STATE_ASSOCIATED)
		link->state = U80211_WPAS_LINK_ASSOCIATED;
	else if (state == U80211_DEVICE_STATE_AUTHENTICATING || state == U80211_DEVICE_STATE_ASSOCIATING)
		link->state = U80211_WPAS_LINK_ASSOCIATING;
	else
		link->state = U80211_WPAS_LINK_DOWN;

	u80211_ap_t *ap = server->device->ap;
	if (ap != NULL) {
		memcpy(link->bssid, ap->mac_address.bytes, sizeof(link->bssid));
		link->ssid_length = (uint8_t)strnlen(ap->ssid, sizeof(ap->ssid) - 1);
		memcpy(link->ssid, ap->ssid, link->ssid_length);
	}
	u80211_kernel_release_spinlock(server->device->association_spinlock);
	return state;
}

static void monitor_operations(u80211_wpas_server_t *server) {
	u80211_wpas_link_t link;
	int state = link_snapshot(server, &link);
	if (server->client_fd < 0 && state == U80211_DEVICE_STATE_ASSOCIATED) {
		u80211_disassociate(server->device);
		server->association_pending = false;
		server->previous_state = U80211_DEVICE_STATE_DOWN;
		return;
	}

	if (server->scan_pending && state != U80211_DEVICE_STATE_SCANNING) {
		server->scan_pending = false;
		send_event(server, U80211_WPAS_EVENT_SCAN_RESULTS, U80211_STATUS_SUCCESS, NULL, 0);
	}

	if (server->association_pending && state != U80211_DEVICE_STATE_AUTHENTICATING && state != U80211_DEVICE_STATE_ASSOCIATING) {
		server->association_pending = false;
		if (state == U80211_DEVICE_STATE_ASSOCIATED) {
			send_event(server, U80211_WPAS_EVENT_ASSOCIATED, U80211_STATUS_SUCCESS, &link, sizeof(link));
			if (server->associated != NULL)
				server->associated(server->associated_context);
		} else {
			send_event(server, U80211_WPAS_EVENT_ASSOCIATION_FAILED, U80211_STATUS_UNKNOWN_ERROR, NULL, 0);
		}
	} else if (server->previous_state == U80211_DEVICE_STATE_ASSOCIATED && state != U80211_DEVICE_STATE_ASSOCIATED) {
		send_event(server, U80211_WPAS_EVENT_DISASSOCIATED, U80211_STATUS_SUCCESS, NULL, 0);
	}

	server->previous_state = state;
}

static int handle_scan_results(u80211_wpas_server_t *server, uint32_t request_id) {
	size_t capacity = u80211_bss_cache_get_count(&server->device->bss_cache);
	u80211_ap_t **aps = capacity == 0 ? NULL : calloc(capacity, sizeof(*aps));
	if (capacity != 0 && aps == NULL)
		return send_response(server, request_id, U80211_STATUS_ENOMEM, NULL, 0);

	size_t count = u80211_bss_cache_get_aps(&server->device->bss_cache, aps, capacity);
	size_t payload_size = sizeof(u80211_wpas_scan_results_t);
	for (size_t i = 0; i < count; ++i) {
		size_t ssid_length = strnlen(aps[i]->ssid, sizeof(aps[i]->ssid) - 1);
		if (aps[i]->rsn_size > UINT16_MAX || payload_size > U80211_WPAS_MAX_PAYLOAD - sizeof(u80211_wpas_scan_record_t) - ssid_length - aps[i]->rsn_size) {
			for (size_t j = 0; j < count; ++j)
				u80211_ap_release(aps[j]);
			free(aps);
			return send_response(server, request_id, U80211_STATUS_NOT_ENOUGH_SPACE, NULL, 0);
		}
		payload_size += sizeof(u80211_wpas_scan_record_t) + ssid_length + aps[i]->rsn_size;
	}

	uint8_t *payload = calloc(1, payload_size);
	if (payload == NULL) {
		for (size_t i = 0; i < count; ++i)
			u80211_ap_release(aps[i]);
		free(aps);
		return send_response(server, request_id, U80211_STATUS_ENOMEM, NULL, 0);
	}
	((u80211_wpas_scan_results_t *)payload)->count = htole32((uint32_t)count);
	size_t offset = sizeof(u80211_wpas_scan_results_t);
	for (size_t i = 0; i < count; ++i) {
		u80211_ap_t *ap = aps[i];
		size_t ssid_length = strnlen(ap->ssid, sizeof(ap->ssid) - 1);
		u80211_wpas_scan_record_t *record = (void *)(payload + offset);
		memcpy(record->bssid, ap->mac_address.bytes, sizeof(record->bssid));
		record->channel = ap->channel;
		record->ssid_length = (uint8_t)ssid_length;
		record->beacon_interval = htole16(ap->interval);
		record->capabilities = htole16(ap->capabilities);
		record->rsn_length = htole16((uint16_t)ap->rsn_size);
		memcpy(record->rate_bitmap, ap->rate_bitmap, sizeof(record->rate_bitmap));
		offset += sizeof(*record);
		memcpy(payload + offset, ap->ssid, ssid_length);
		offset += ssid_length;
		if (ap->rsn_size != 0) {
			memcpy(payload + offset, ap->rsn, ap->rsn_size);
			offset += ap->rsn_size;
		}
		u80211_ap_release(ap);
	}
	free(aps);
	int result = send_response(server, request_id, U80211_STATUS_SUCCESS, payload, (uint32_t)payload_size);
	free(payload);
	return result;
}

static int handle_associate(u80211_wpas_server_t *server, uint32_t request_id, const uint8_t *payload, uint32_t payload_length) {
	if (payload_length < sizeof(u80211_wpas_associate_t))
		return send_response(server, request_id, U80211_STATUS_NOT_PERMITTED, NULL, 0);
	const u80211_wpas_associate_t *request = (const void *)payload;
	size_t ies_length = le16toh(request->information_elements_length);
	if (ies_length != payload_length - sizeof(*request))
		return send_response(server, request_id, U80211_STATUS_NOT_PERMITTED, NULL, 0);

	u80211_mac_address_t bssid;
	memcpy(bssid.bytes, request->bssid, sizeof(bssid.bytes));
	u80211_ap_t *ap = u80211_bss_cache_find(&server->device->bss_cache, &bssid);
	if (ap == NULL)
		return send_response(server, request_id, U80211_STATUS_NOT_PERMITTED, NULL, 0);

	int status;
	if (ap->rsn_size != 0 || (ap->capabilities & IEEE80211_CAPABILITY_PRIVACY) != 0)
		status = U80211_STATUS_UNSUPPORTED;
	else
		status = u80211_associate(server->device, ap, payload + sizeof(*request), ies_length);
	u80211_ap_release(ap);
	if (status == U80211_STATUS_SUCCESS)
		server->association_pending = true;
	return send_response(server, request_id, status, NULL, 0);
}

static int handle_request(u80211_wpas_server_t *server, const uint8_t *packet, size_t packet_size) {
	if (packet_size < sizeof(u80211_wpas_header_t))
		return -1;
	const u80211_wpas_header_t *header = (const void *)packet;
	uint32_t payload_length = le32toh(header->payload_length);
	uint32_t request_id = le32toh(header->request_id);
	uint16_t type = le16toh(header->type);
	if (le32toh(header->magic) != U80211_WPAS_PROTOCOL_MAGIC ||
		le16toh(header->version) != U80211_WPAS_PROTOCOL_VERSION ||
		payload_length > U80211_WPAS_MAX_PAYLOAD ||
		packet_size != sizeof(*header) + payload_length || request_id == 0)
		return -1;
	const uint8_t *payload = packet + sizeof(*header);

	switch (type) {
		case U80211_WPAS_REQUEST_HELLO: {
			if (payload_length != 0)
				return -1;
			u80211_wpas_hello_t hello = {0};
			memcpy(hello.mac_address, server->device->metadata.mac_address.bytes, sizeof(hello.mac_address));
			return send_response(server, request_id, U80211_STATUS_SUCCESS, &hello, sizeof(hello));
		}
		case U80211_WPAS_REQUEST_SCAN: {
			if (payload_length != 0)
				return -1;
			int status = u80211_scan(server->device);
			if (status == U80211_STATUS_SUCCESS)
				server->scan_pending = true;
			return send_response(server, request_id, status, NULL, 0);
		}
		case U80211_WPAS_REQUEST_GET_SCAN_RESULTS:
			return payload_length == 0 ? handle_scan_results(server, request_id) : -1;
		case U80211_WPAS_REQUEST_ASSOCIATE:
			return handle_associate(server, request_id, payload, payload_length);
		case U80211_WPAS_REQUEST_DISASSOCIATE: {
			if (payload_length != 0)
				return -1;
			int status = u80211_disassociate(server->device);
			int result = send_response(server, request_id, status, NULL, 0);
			if (status == U80211_STATUS_SUCCESS) {
				server->association_pending = false;
				server->previous_state = U80211_DEVICE_STATE_DOWN;
				send_event(server, U80211_WPAS_EVENT_DISASSOCIATED, status, NULL, 0);
			}
			return result;
		}
		case U80211_WPAS_REQUEST_GET_LINK: {
			if (payload_length != 0)
				return -1;
			u80211_wpas_link_t link;
			link_snapshot(server, &link);
			return send_response(server, request_id, U80211_STATUS_SUCCESS, &link, sizeof(link));
		}
		default:
			return send_response(server, request_id, U80211_STATUS_UNSUPPORTED, NULL, 0);
	}
}

static void close_client(u80211_wpas_server_t *server) {
	if (server->client_fd >= 0) {
		close(server->client_fd);
		server->client_fd = -1;
	}
	server->scan_pending = false;
	server->association_pending = false;
}

static void *server_loop(void *context) {
	u80211_wpas_server_t *server = context;
	size_t packet_capacity = sizeof(u80211_wpas_header_t) + U80211_WPAS_MAX_PAYLOAD;
	uint8_t *packet = malloc(packet_capacity);
	if (packet == NULL)
		return NULL;

	while (!server_stopping(server)) {
		struct pollfd descriptors[2] = {
			{ .fd = server->listen_fd, .events = POLLIN },
			{ .fd = server->client_fd, .events = POLLIN },
		};
		int count = poll(descriptors, 2, SERVER_POLL_TIMEOUT_MS);
		if (count < 0) {
			if (errno == EINTR)
				continue;
			break;
		}
		if (descriptors[0].revents & POLLIN) {
			int client = accept4(server->listen_fd, NULL, NULL, SOCK_CLOEXEC);
			if (client >= 0) {
				if (server->client_fd >= 0)
					close(client);
				else
					server->client_fd = client;
			}
		}
		if (server->client_fd >= 0 && descriptors[1].revents != 0) {
			ssize_t received = recv(server->client_fd, packet, packet_capacity, 0);
			if (received <= 0 || (descriptors[1].revents & (POLLERR | POLLHUP | POLLNVAL)) ||
				handle_request(server, packet, (size_t)received) != 0)
				close_client(server);
		}
		monitor_operations(server);
	}

	free(packet);
	close_client(server);
	return NULL;
}

static int remove_stale_socket(void) {
	struct stat socket_stat;
	if (lstat(U80211_WPAS_SOCKET_PATH, &socket_stat) != 0)
		return errno == ENOENT ? 0 : -1;
	if (!S_ISSOCK(socket_stat.st_mode)) {
		errno = EEXIST;
		return -1;
	}

	int probe = socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0);
	if (probe < 0)
		return -1;
	struct sockaddr_un address = { .sun_family = AF_UNIX };
	strncpy(address.sun_path, U80211_WPAS_SOCKET_PATH, sizeof(address.sun_path) - 1);
	int result = connect(probe, (struct sockaddr *)&address, sizeof(address));
	int connect_errno = errno;
	close(probe);
	if (result == 0 || connect_errno != ECONNREFUSED) {
		errno = result == 0 ? EADDRINUSE : connect_errno;
		return -1;
	}
	return unlink(U80211_WPAS_SOCKET_PATH);
}

int u80211_wpas_server_start(u80211_device_t *device, u80211_wpas_associated_fn_t associated, void *associated_context, u80211_wpas_server_t **server_out) {
	if (device == NULL || server_out == NULL)
		return -1;
	if (remove_stale_socket() != 0)
		return -1;

	u80211_wpas_server_t *server = calloc(1, sizeof(*server));
	if (server == NULL)
		return -1;
	server->device = device;
	server->listen_fd = -1;
	server->client_fd = -1;
	server->previous_state = u80211_get_device_state(device);
	server->associated = associated;
	server->associated_context = associated_context;

	server->listen_fd = socket(AF_UNIX, SOCK_SEQPACKET | SOCK_CLOEXEC, 0);
	if (server->listen_fd < 0)
		goto fail;
	struct sockaddr_un address = { .sun_family = AF_UNIX };
	strncpy(address.sun_path, U80211_WPAS_SOCKET_PATH, sizeof(address.sun_path) - 1);
	if (bind(server->listen_fd, (struct sockaddr *)&address, sizeof(address)) != 0 ||
		chmod(U80211_WPAS_SOCKET_PATH, S_IRUSR | S_IWUSR) != 0 || listen(server->listen_fd, 1) != 0)
		goto fail;
	if (pthread_create(&server->thread, NULL, server_loop, server) != 0)
		goto fail;

	*server_out = server;
	return 0;

fail:
	if (server->listen_fd >= 0)
		close(server->listen_fd);
	unlink(U80211_WPAS_SOCKET_PATH);
	free(server);
	return -1;
}

void u80211_wpas_server_stop(u80211_wpas_server_t *server) {
	if (server == NULL)
		return;
	__atomic_store_n(&server->stopping, true, __ATOMIC_RELEASE);
	pthread_join(server->thread, NULL);
	if (server->listen_fd >= 0)
		close(server->listen_fd);
	unlink(U80211_WPAS_SOCKET_PATH);
	free(server);
}
