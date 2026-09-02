#ifndef U80211_DRV_RTL8188EU_H
#define U80211_DRV_RTL8188EU_H

#include <stdint.h>

#include <u80211_drv/kernel_interface.h>

#define U80211_DRV_RTL8188EU_REG_SYS_ISO_CTRL 0x0000
#define U80211_DRV_RTL8188EU_REG_SYS_ISO_CTRL_PWC_EV12V (1u << 15)

#define U80211_DRV_RTL8188EU_REG_SYS_FUNC 0x0002
#define U80211_DRV_RTL8188EU_REG_SYS_FUNC_ELDR (1u << 12)

#define U80211_DRV_RTL8188EU_REG_SYS_CLKR 0x0008
#define U80211_DRV_RTL8188EU_REG_SYS_CLKR_ANA8M (1u << 1)
#define U80211_DRV_RTL8188EU_REG_SYS_CLKR_LOADER_ENABLE (1u << 5)

#define U80211_DRV_RTL8188EU_REG_9346CR 0x000a
#define U80211_DRV_RTL8188EU_REG_9346CR_EEPROM_BOOT (1u << 4)
#define U80211_DRV_RTL8188EU_REG_9346CR_EEPROM_ENABLE (1u << 5)

#define U80211_DRV_RTL8188EU_REG_EFUSE_CTRL 0x0030

#define U80211_DRV_RTL8188EU_REG_EFUSE_ACCESS 0x00cf
#define U80211_DRV_RTL8188EU_EFUSE_ACCESS_ENABLE 0x69
#define U80211_DRV_RTL8188EU_EFUSE_ACCESS_DISABLE 0x00

#define U80211_DRV_RTL8188EU_REG_SYS_CFG 0xf0
#define U80211_DRV_RTL8188EU_REG_SYS_CFG_TRP_VAUX_EN (1u << 23)
#define U80211_DRV_RTL8188EU_REG_SYS_CFG_VER(value) (((value) >> 12) & 0xfu)

int u80211_drv_rtl8188eu_efuse_prepare(u80211_drv_device_handle_t device);
int u80211_drv_rtl8188eu_reg_read8(u80211_drv_device_handle_t device, uint16_t reg, uint8_t *value);
int u80211_drv_rtl8188eu_reg_read16(u80211_drv_device_handle_t device, uint16_t reg, uint16_t *value);
int u80211_drv_rtl8188eu_reg_read32(u80211_drv_device_handle_t device, uint16_t reg, uint32_t *value);
int u80211_drv_rtl8188eu_reg_write8(u80211_drv_device_handle_t device, uint16_t reg, uint8_t value);
int u80211_drv_rtl8188eu_reg_write16(u80211_drv_device_handle_t device, uint16_t reg, uint16_t value);
int u80211_drv_rtl8188eu_reg_write32(u80211_drv_device_handle_t device, uint16_t reg, uint32_t value);

#endif
