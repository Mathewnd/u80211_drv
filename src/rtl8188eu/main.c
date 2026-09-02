#include <stdio.h>

#include <u80211_drv/drv_init.h>
#include <u80211_drv/rtl8188eu.h>
#include <u80211_drv/status.h>

int u80211_drv_rtl8188eu_init(u80211_drv_device_handle_t device, u80211_drv_interface_handle_t interface) {
	(void)interface;
	uint32_t sys_cfg;
	int status;

	status = u80211_drv_rtl8188eu_reg_read32(device, U80211_DRV_RTL8188EU_REG_SYS_CFG, &sys_cfg);
	if (status != U80211_DRV_STATUS_SUCCESS)
		return status;

	printf("REG_SYS_CFG: 0x%08x\n", sys_cfg);
	return U80211_DRV_STATUS_SUCCESS;
}
