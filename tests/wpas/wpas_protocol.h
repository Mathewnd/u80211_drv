#ifndef U80211_TEST_WPAS_PROTOCOL_H
#define U80211_TEST_WPAS_PROTOCOL_H

#include <stdint.h>

#define U80211_WPAS_SOCKET_PATH "/tmp/.u80211_sock"
#define U80211_WPAS_PROTOCOL_MAGIC UINT32_C(0x55383032)
#define U80211_WPAS_PROTOCOL_VERSION UINT16_C(1)
#define U80211_WPAS_MAX_PAYLOAD UINT32_C(65535)

enum u80211_wpas_message_type {
	U80211_WPAS_REQUEST_HELLO = 1,
	U80211_WPAS_REQUEST_SCAN = 2,
	U80211_WPAS_REQUEST_GET_SCAN_RESULTS = 3,
	U80211_WPAS_REQUEST_ASSOCIATE = 4,
	U80211_WPAS_REQUEST_DISASSOCIATE = 5,
	U80211_WPAS_REQUEST_GET_LINK = 6,

	U80211_WPAS_RESPONSE = 0x100,

	U80211_WPAS_EVENT_SCAN_RESULTS = 0x200,
	U80211_WPAS_EVENT_ASSOCIATED = 0x201,
	U80211_WPAS_EVENT_ASSOCIATION_FAILED = 0x202,
	U80211_WPAS_EVENT_DISASSOCIATED = 0x203,
};

/* All integer fields in the wire structures are little endian. */
typedef struct __attribute__((packed)) {
	uint32_t magic;
	uint16_t version;
	uint16_t type;
	uint32_t request_id;
	uint32_t payload_length;
	int32_t status;
} u80211_wpas_header_t;

typedef struct __attribute__((packed)) {
	uint8_t mac_address[6];
	uint16_t reserved;
} u80211_wpas_hello_t;

typedef struct __attribute__((packed)) {
	uint8_t bssid[6];
	uint16_t information_elements_length;
	/* Association-request information elements follow. */
} u80211_wpas_associate_t;

typedef struct __attribute__((packed)) {
	uint32_t count;
	/* A sequence of u80211_wpas_scan_record_t records follows. */
} u80211_wpas_scan_results_t;

typedef struct __attribute__((packed)) {
	uint8_t bssid[6];
	uint8_t channel;
	uint8_t ssid_length;
	uint16_t beacon_interval;
	uint16_t capabilities;
	uint16_t rsn_length;
	uint8_t rate_bitmap[16];
	/* SSID bytes followed by the RSN body (without IE id/length) follow. */
} u80211_wpas_scan_record_t;

enum u80211_wpas_link_state {
	U80211_WPAS_LINK_DOWN = 0,
	U80211_WPAS_LINK_ASSOCIATING = 1,
	U80211_WPAS_LINK_ASSOCIATED = 2,
};

typedef struct __attribute__((packed)) {
	uint8_t state;
	uint8_t bssid[6];
	uint8_t ssid_length;
	uint8_t ssid[32];
} u80211_wpas_link_t;

#endif
