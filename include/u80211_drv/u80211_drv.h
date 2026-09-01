#ifndef U80211_DRV_U80211_DRV_H
#define U80211_DRV_U80211_DRV_H

#include <stdint.h>

#include <u80211_drv/kernel_interface.h>

#ifdef __cplusplus
extern "C" {
#endif

int u80211_drv_probe(u80211_drv_device_handle_t device, u80211_drv_interface_handle_t interface);
int u80211_drv_attach(u80211_drv_device_handle_t device, u80211_drv_interface_handle_t interface);

#ifdef __cplusplus
}
#endif

#endif
