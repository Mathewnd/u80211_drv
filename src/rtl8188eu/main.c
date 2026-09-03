#include <stdio.h>

#include <u80211_drv/drv_init.h>
#include <u80211_drv/rtl8188eu.h>
#include <u80211_drv/status.h>

static void firmware_loaded(void *context, const void *firmware_data, size_t firmware_size) {
	u80211_drv_rtl8188eu_t *rtl8188eu = context;

	u80211_drv_kernel_print(U80211_DRV_KERNEL_PRINT_LEVEL_INFO, "rtl8188eu: found rtl8188eufw.bin");
	int status = u80211_drv_rtl8188eu_power_active(rtl8188eu->device);
	if (status != U80211_DRV_STATUS_SUCCESS)
		goto error;

	status = u80211_drv_rtl8188eu_mac_enable_infrastructure(rtl8188eu->device);
	if (status != U80211_DRV_STATUS_SUCCESS)
		goto error;

	(void)firmware_data;
	(void)firmware_size;
	return;

error:
	u80211_drv_kernel_print(U80211_DRV_KERNEL_PRINT_LEVEL_ERROR, "rtl8188eu: post firmware initialization failed");
}

int u80211_drv_rtl8188eu_init(u80211_drv_device_handle_t device, u80211_drv_interface_handle_t interface) {
	uint32_t sys_cfg;
	int status;

	status = u80211_drv_rtl8188eu_reg_read32(device, U80211_DRV_RTL8188EU_REG_SYS_CFG, &sys_cfg);
	if (status != U80211_DRV_STATUS_SUCCESS)
		return status;

	if ((sys_cfg & U80211_DRV_RTL8188EU_REG_SYS_CFG_TRP_VAUX_EN) != 0 || U80211_DRV_RTL8188EU_REG_SYS_CFG_VER(sys_cfg) == 8)
		return U80211_DRV_STATUS_NOT_SUPPORTED;

	u80211_drv_rtl8188eu_t *rtl8188eu = u80211_drv_kernel_allocate(sizeof(*rtl8188eu));
	if (rtl8188eu == NULL)
		return U80211_DRV_STATUS_OUT_OF_MEMORY;

	rtl8188eu->device = device;
	rtl8188eu->interface = interface;

	uint8_t *efuse_map = u80211_drv_kernel_allocate(U80211_DRV_RTL8188EU_EFUSE_MAP_LEN);
	if (efuse_map == NULL) {
		u80211_drv_kernel_free(rtl8188eu);
		return U80211_DRV_STATUS_OUT_OF_MEMORY;
	}

	status = u80211_drv_rtl8188eu_efuse_prepare(device);
	if (status != U80211_DRV_STATUS_SUCCESS) {
		u80211_drv_kernel_free(efuse_map);
		u80211_drv_kernel_free(rtl8188eu);
		return status;
	}

	status = u80211_drv_rtl8188eu_read_efuse(device, efuse_map);
	if (status != U80211_DRV_STATUS_SUCCESS) {
		u80211_drv_kernel_free(efuse_map);
		u80211_drv_kernel_free(rtl8188eu);
		return status;
	}

	status = u80211_drv_rtl8188eu_efuse_finish(device);
	if (status != U80211_DRV_STATUS_SUCCESS) {
		u80211_drv_kernel_free(efuse_map);
		u80211_drv_kernel_free(rtl8188eu);
		return status;
	}

	status = u80211_drv_rtl8188eu_parse_efuse(efuse_map, &rtl8188eu->efuse);
	if (status != U80211_DRV_STATUS_SUCCESS) {
		u80211_drv_kernel_free(efuse_map);
		u80211_drv_kernel_free(rtl8188eu);
		return status;
	}

	u80211_drv_kernel_free(efuse_map);

	// TODO: have a way of cancelling this wait for detach
	u80211_drv_kernel_print(U80211_DRV_KERNEL_PRINT_LEVEL_INFO, "rtl8188eu: waiting for rtl8188eufw.bin");
	status = u80211_drv_kernel_get_firmware("rtl8188eufw.bin", firmware_loaded, rtl8188eu);
	if (status != U80211_DRV_STATUS_SUCCESS)
		u80211_drv_kernel_free(rtl8188eu);

	return status;
}
