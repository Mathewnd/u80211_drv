# u80211_drv

u80211_drv is a portable implementation of 802.11 NIC drivers.

This is still experimental, so use with caution.

Only currently supported NIC is the rtl8188eu (tested with DWA-123 D1 dongle)

# Higher layer
An easy-to-integrate 802.11 layer is available in the [u80211 repository](https://github.com/mathewnd/u80211).

# How to port

1. Add the sources under src/ to your build
2. Add the headers under include/ to your build
3. Implement the kernel API declared in [`include/u80211_drv/kernel_interface.h`](include/u80211_drv/kernel_interface.h)

# Example probe+attach usage

```c
#include <u80211_drv/u80211_drv.h>

static int probe_and_attach(u80211_drv_device_handle_t device,
		u80211_drv_interface_handle_t interface) {
	int status = u80211_drv_probe(device, interface);
	if (status != U80211_DRV_STATUS_SUCCESS)
		return status;

	return u80211_drv_attach(device, interface);
}
```
