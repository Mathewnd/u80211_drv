#include <stdint.h>

#include <u80211_drv/kernel_interface.h>
#include <u80211_drv/rtl8188eu.h>
#include <u80211_drv/status.h>

int u80211_drv_rtl8188eu_efuse_prepare(u80211_drv_device_handle_t device) {
	status = u80211_drv_rtl8188eu_reg_write8(device, U80211_DRV_RTL8188EU_REG_EFUSE_ACCESS, U80211_DRV_RTL8188EU_EFUSE_ACCESS_ENABLE);
	if (status != U80211_DRV_STATUS_SUCCESS)
		return status;

	status = u80211_drv_rtl8188eu_reg_read16(device, U80211_DRV_RTL8188EU_REG_SYS_ISO_CTRL, &value);
	if (status != U80211_DRV_STATUS_SUCCESS)
		return status;

	if ((value & U80211_DRV_RTL8188EU_REG_SYS_ISO_CTRL_PWC_EV12V) == 0) {
		value |= U80211_DRV_RTL8188EU_REG_SYS_ISO_CTRL_PWC_EV12V;
		status = u80211_drv_rtl8188eu_reg_write16(device, U80211_DRV_RTL8188EU_REG_SYS_ISO_CTRL, value);
		if (status != U80211_DRV_STATUS_SUCCESS)
			return status;
	}

	status = u80211_drv_rtl8188eu_reg_read16(device, U80211_DRV_RTL8188EU_REG_SYS_FUNC, &value);
	if (status != U80211_DRV_STATUS_SUCCESS)
		return status;

	if ((value & U80211_DRV_RTL8188EU_REG_SYS_FUNC_ELDR) == 0) {
		value |= U80211_DRV_RTL8188EU_REG_SYS_FUNC_ELDR;
		status = u80211_drv_rtl8188eu_reg_write16(device, U80211_DRV_RTL8188EU_REG_SYS_FUNC, value);
		if (status != U80211_DRV_STATUS_SUCCESS)
			return status;
	}

	status = u80211_drv_rtl8188eu_reg_read16(device, U80211_DRV_RTL8188EU_REG_SYS_CLKR, &value);
	if (status != U80211_DRV_STATUS_SUCCESS)
		return status;

	uint16_t required_clocks = U80211_DRV_RTL8188EU_REG_SYS_CLKR_LOADER_ENABLE | U80211_DRV_RTL8188EU_REG_SYS_CLKR_ANA8M;
	if ((value & required_clocks) != required_clocks) {
		value |= required_clocks;
		status = u80211_drv_rtl8188eu_reg_write16(device, U80211_DRV_RTL8188EU_REG_SYS_CLKR, value);
		if (status != U80211_DRV_STATUS_SUCCESS)
			return status;
	}

	return U80211_DRV_STATUS_SUCCESS;
}
