#ifndef U80211_DRV_TEST_WPAS_SERVER_H
#define U80211_DRV_TEST_WPAS_SERVER_H

#include <u80211/u80211.h>

typedef struct u80211_wpas_server u80211_wpas_server_t;
typedef void (*u80211_wpas_associated_fn_t)(void *context);

int u80211_wpas_server_start(u80211_device_t *device, u80211_wpas_associated_fn_t associated, void *associated_context, u80211_wpas_server_t **server_out);
void u80211_wpas_server_stop(u80211_wpas_server_t *server);

#endif
