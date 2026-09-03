#include <stddef.h>
#include <stdint.h>

#include <u80211_drv/rtl8188eu.h>
#include <u80211_drv/status.h>

#define RTL8188EU_FIRMWARE_SIGNATURE  0x88e
#define TL8188EU_FIRMWARE_PREPARE_DELAY_US  50

static int firmware_reset(u80211_drv_device_handle_t device) {
	// hold mcu wrapper
	uint8_t value8;
	int status = u80211_drv_rtl8188eu_reg_read8(device, U80211_DRV_RTL8188EU_REG_RSV_CTRL, &value8);
	if (status != U80211_DRV_STATUS_SUCCESS)
		return status;

	value8 &= ~U80211_DRV_RTL8188EU_REG_RSV_CTRL_WLOCK_00;
	status = u80211_drv_rtl8188eu_reg_write8(device, U80211_DRV_RTL8188EU_REG_RSV_CTRL, value8);
	if (status != U80211_DRV_STATUS_SUCCESS)
		return status;

	// assert wrapper reset
	uint16_t value16;
	status = u80211_drv_rtl8188eu_reg_read16(device, U80211_DRV_RTL8188EU_REG_RSV_CTRL, &value16);
	if (status != U80211_DRV_STATUS_SUCCESS)
		return status;

	value16 &= ~U80211_DRV_RTL8188EU_REG_RSV_CTRL_MCU_RST;
	status = u80211_drv_rtl8188eu_reg_write16(device, U80211_DRV_RTL8188EU_REG_RSV_CTRL, value16);
	if (status != U80211_DRV_STATUS_SUCCESS)
		return status;

	// assert cpu reset
	status = u80211_drv_rtl8188eu_reg_read16(device, U80211_DRV_RTL8188EU_REG_SYS_FUNC, &value16);
	if (status != U80211_DRV_STATUS_SUCCESS)
		return status;

	value16 &= ~U80211_DRV_RTL8188EU_REG_SYS_FUNC_CPUEN;
	status = u80211_drv_rtl8188eu_reg_write16(device, U80211_DRV_RTL8188EU_REG_SYS_FUNC, value16);
	if (status != U80211_DRV_STATUS_SUCCESS)
		return status;

	// hold mcu wrapper
	status = u80211_drv_rtl8188eu_reg_read8(device, U80211_DRV_RTL8188EU_REG_RSV_CTRL, &value8);
	if (status != U80211_DRV_STATUS_SUCCESS)
		return status;

	value8 &= ~U80211_DRV_RTL8188EU_REG_RSV_CTRL_WLOCK_00;
	status = u80211_drv_rtl8188eu_reg_write8(device, U80211_DRV_RTL8188EU_REG_RSV_CTRL, value8);
	if (status != U80211_DRV_STATUS_SUCCESS)
		return status;

	// deassert mcu wrapper reset
	status = u80211_drv_rtl8188eu_reg_read16(device, U80211_DRV_RTL8188EU_REG_RSV_CTRL, &value16);
	if (status != U80211_DRV_STATUS_SUCCESS)
		return status;

	value16 |= U80211_DRV_RTL8188EU_REG_RSV_CTRL_MCU_RST;
	status = u80211_drv_rtl8188eu_reg_write16(device, U80211_DRV_RTL8188EU_REG_RSV_CTRL, value16);
	if (status != U80211_DRV_STATUS_SUCCESS)
		return status;

	// deassert cpu reset
	status = u80211_drv_rtl8188eu_reg_read16(device, U80211_DRV_RTL8188EU_REG_SYS_FUNC, &value16);
	if (status != U80211_DRV_STATUS_SUCCESS)
		return status;

	value16 |= U80211_DRV_RTL8188EU_REG_SYS_FUNC_CPUEN;
	return u80211_drv_rtl8188eu_reg_write16(device, U80211_DRV_RTL8188EU_REG_SYS_FUNC, value16);
}

int u80211_drv_rtl8188eu_firmware_prepare(u80211_drv_device_handle_t device, const void *firmware_data, size_t firmware_size, const uint8_t **firmware_payload, size_t *firmware_payload_size) {
	// verify if the firmware is ok
	const uint8_t *payload = firmware_data;
	size_t payload_size = firmware_size;
	uint16_t signature = (uint16_t)payload[0] | ((uint16_t)payload[1] << 8);
	if ((signature >> 4) == RTL8188EU_FIRMWARE_SIGNATURE) {
		payload += U80211_DRV_RTL8188EU_FIRMWARE_HEADER_SIZE;
		payload_size -= U80211_DRV_RTL8188EU_FIRMWARE_HEADER_SIZE;
	}

	// check if there is stale ram in the mcu
	uint8_t value8;
	int status = u80211_drv_rtl8188eu_reg_read8(device, U80211_DRV_RTL8188EU_REG_MCUFWDL, &value8);
	if (status != U80211_DRV_STATUS_SUCCESS)
		return status;

	if ((value8 & U80211_DRV_RTL8188EU_REG_MCUFWDL_RAM_DOWNLOAD_SELECT) != 0) {
		// clear stale ram and reset firmware
		status = u80211_drv_rtl8188eu_reg_write8(device, U80211_DRV_RTL8188EU_REG_MCUFWDL, 0);
		if (status != U80211_DRV_STATUS_SUCCESS)
			return status;

		status = firmware_reset(device);
		if (status != U80211_DRV_STATUS_SUCCESS)
			return status;
	}

	// enable firmware download
	status = u80211_drv_rtl8188eu_reg_read8(device, U80211_DRV_RTL8188EU_REG_MCUFWDL, &value8);
	if (status != U80211_DRV_STATUS_SUCCESS)
		return status;

	value8 |= U80211_DRV_RTL8188EU_REG_MCUFWDL_ENABLE;
	status = u80211_drv_rtl8188eu_reg_write8(device, U80211_DRV_RTL8188EU_REG_MCUFWDL, value8);
	if (status != U80211_DRV_STATUS_SUCCESS)
		return status;

	uint32_t value32;
	status = u80211_drv_rtl8188eu_reg_read32(device, U80211_DRV_RTL8188EU_REG_MCUFWDL, &value32);
	if (status != U80211_DRV_STATUS_SUCCESS)
		return status;

	value32 &= ~U80211_DRV_RTL8188EU_REG_MCUFWDL_ROM_DOWNLOAD_LENGTH;
	status = u80211_drv_rtl8188eu_reg_write32(device, U80211_DRV_RTL8188EU_REG_MCUFWDL, value32);
	if (status != U80211_DRV_STATUS_SUCCESS)
		return status;

	// reset firmware checksum
	status = u80211_drv_rtl8188eu_reg_read8(device, U80211_DRV_RTL8188EU_REG_MCUFWDL, &value8);
	if (status != U80211_DRV_STATUS_SUCCESS)
		return status;

	value8 |= U80211_DRV_RTL8188EU_REG_MCUFWDL_CHECKSUM_REPORT;
	status = u80211_drv_rtl8188eu_reg_write8(device, U80211_DRV_RTL8188EU_REG_MCUFWDL, value8);
	if (status != U80211_DRV_STATUS_SUCCESS)
		return status;

	u80211_drv_kernel_stall_us(RTL8188EU_FIRMWARE_PREPARE_DELAY_US);
	*firmware_payload = payload;
	*firmware_payload_size = payload_size;
	return U80211_DRV_STATUS_SUCCESS;
}
