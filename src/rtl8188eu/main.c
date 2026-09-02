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

	if ((sys_cfg & U80211_DRV_RTL8188EU_REG_SYS_CFG_TRP_VAUX_EN) != 0 || U80211_DRV_RTL8188EU_REG_SYS_CFG_VER(sys_cfg) == 8)
		return U80211_DRV_STATUS_NOT_SUPPORTED;

	uint8_t *efuse_map = u80211_drv_kernel_allocate(U80211_DRV_RTL8188EU_EFUSE_MAP_LEN);
	if (efuse_map == NULL)
		return U80211_DRV_STATUS_OUT_OF_MEMORY;

	status = u80211_drv_rtl8188eu_efuse_prepare(device);
	if (status != U80211_DRV_STATUS_SUCCESS) {
		u80211_drv_kernel_free(efuse_map);
		return status;
	}

	status = u80211_drv_rtl8188eu_read_efuse(device, efuse_map);
	if (status != U80211_DRV_STATUS_SUCCESS) {
		u80211_drv_kernel_free(efuse_map);
		return status;
	}

	status = u80211_drv_rtl8188eu_efuse_finish(device);
	if (status != U80211_DRV_STATUS_SUCCESS) {
		u80211_drv_kernel_free(efuse_map);
		return status;
	}

	u80211_drv_rtl8188eu_efuse_t efuse;
	status = u80211_drv_rtl8188eu_parse_efuse(efuse_map, &efuse);
	if (status != U80211_DRV_STATUS_SUCCESS) {
		u80211_drv_kernel_free(efuse_map);
		return status;
	}

	u80211_drv_kernel_free(efuse_map);

	return U80211_DRV_STATUS_SUCCESS;
}
