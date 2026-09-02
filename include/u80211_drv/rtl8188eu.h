#ifndef U80211_DRV_RTL8188EU_H
#define U80211_DRV_RTL8188EU_H

#include <stdint.h>

#include <u80211_drv/kernel_interface.h>

#define U80211_DRV_RTL8188EU_REG_SYS_CFG 0xf0
#define U80211_DRV_RTL8188EU_REG_SYS_CFG_TRP_VAUX_EN (1u << 23)
#define U80211_DRV_RTL8188EU_REG_SYS_CFG_VER(value) (((value) >> 12) & 0xfu)

int u80211_drv_rtl8188eu_reg_read8(u80211_drv_device_handle_t device, uint16_t reg, uint8_t *value);
int u80211_drv_rtl8188eu_reg_read16(u80211_drv_device_handle_t device, uint16_t reg, uint16_t *value);
int u80211_drv_rtl8188eu_reg_read32(u80211_drv_device_handle_t device, uint16_t reg, uint32_t *value);
int u80211_drv_rtl8188eu_reg_write8(u80211_drv_device_handle_t device, uint16_t reg, uint8_t value);
int u80211_drv_rtl8188eu_reg_write16(u80211_drv_device_handle_t device, uint16_t reg, uint16_t value);
int u80211_drv_rtl8188eu_reg_write32(u80211_drv_device_handle_t device, uint16_t reg, uint32_t value);

#endif
