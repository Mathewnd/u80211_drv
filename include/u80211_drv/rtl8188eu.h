#ifndef U80211_DRV_RTL8188EU_H
#define U80211_DRV_RTL8188EU_H

#include <stdint.h>

#include <u80211_drv/kernel_interface.h>

#define U80211_DRV_RTL8188EU_REG_SYS_ISO_CTRL 0x0000
#define U80211_DRV_RTL8188EU_REG_SYS_ISO_CTRL_PWC_EV12V (1u << 15)

#define U80211_DRV_RTL8188EU_REG_SYS_FUNC 0x0002
#define U80211_DRV_RTL8188EU_REG_SYS_FUNC_BBRSTB (1u << 0)
#define U80211_DRV_RTL8188EU_REG_SYS_FUNC_BB_GLB_RSTN (1u << 1)
#define U80211_DRV_RTL8188EU_REG_SYS_FUNC_ELDR (1u << 12)

#define U80211_DRV_RTL8188EU_REG_APS_FSMCO 0x0004
#define U80211_DRV_RTL8188EU_REG_APS_FSMCO_MAC_ENABLE (1u << 8)
#define U80211_DRV_RTL8188EU_REG_APS_FSMCO_HW_SUSPEND (1u << 11)
#define U80211_DRV_RTL8188EU_REG_APS_FSMCO_PCIE (1u << 12)
#define U80211_DRV_RTL8188EU_REG_APS_FSMCO_HW_POWERDOWN (1u << 15)
#define U80211_DRV_RTL8188EU_REG_APS_FSMCO_POWER_READY (1u << 17)

#define U80211_DRV_RTL8188EU_REG_SYS_CLKR 0x0008
#define U80211_DRV_RTL8188EU_REG_SYS_CLKR_ANA8M (1u << 1)
#define U80211_DRV_RTL8188EU_REG_SYS_CLKR_LOADER_ENABLE (1u << 5)

#define U80211_DRV_RTL8188EU_REG_9346CR 0x000a
#define U80211_DRV_RTL8188EU_REG_9346CR_EEPROM_BOOT (1u << 4)
#define U80211_DRV_RTL8188EU_REG_9346CR_EEPROM_ENABLE (1u << 5)

#define U80211_DRV_RTL8188EU_REG_LPLDO_CTRL 0x0023
#define U80211_DRV_RTL8188EU_REG_LPLDO_CTRL_SLEEP (1u << 4)

#define U80211_DRV_RTL8188EU_REG_AFE_XTAL_CTRL 0x0024
#define U80211_DRV_RTL8188EU_REG_AFE_XTAL_CTRL_SCHMITT_TRIGGER (1u << 23)

#define U80211_DRV_RTL8188EU_REG_EFUSE_CTRL 0x0030
#define U80211_DRV_RTL8188EU_REG_EFUSE_CTRL_READ_READY (1u << 31)

#define U80211_DRV_RTL8188EU_REG_EFUSE_ACCESS 0x00cf
#define U80211_DRV_RTL8188EU_EFUSE_ACCESS_ENABLE 0x69
#define U80211_DRV_RTL8188EU_EFUSE_ACCESS_DISABLE 0x00

#define U80211_DRV_RTL8188EU_EFUSE_PHYSICAL_LEN 512
#define U80211_DRV_RTL8188EU_EFUSE_MAP_LEN 512
#define U80211_DRV_RTL8188EU_EFUSE_WORDS_PER_SECTION 4
#define U80211_DRV_RTL8188EU_EFUSE_RTL_ID 0x8129
#define U80211_DRV_RTL8188EU_MAC_ADDRESS_LEN 6
#define U80211_DRV_RTL8188EU_CCK_TX_POWER_BASE_INDEX_COUNT 6
#define U80211_DRV_RTL8188EU_HT40_1S_TX_POWER_BASE_INDEX_COUNT 5

#define U80211_DRV_RTL8188EU_REG_SYS_CFG 0xf0
#define U80211_DRV_RTL8188EU_REG_SYS_CFG_TRP_VAUX_EN (1u << 23)
#define U80211_DRV_RTL8188EU_REG_SYS_CFG_VER(value) (((value) >> 12) & 0xfu)

#define U80211_DRV_RTL8188EU_REG_CR 0x0100
#define U80211_DRV_RTL8188EU_REG_CR_HCI_TXDMA_ENABLE (1u << 0)
#define U80211_DRV_RTL8188EU_REG_CR_HCI_RXDMA_ENABLE (1u << 1)
#define U80211_DRV_RTL8188EU_REG_CR_TXDMA_ENABLE (1u << 2)
#define U80211_DRV_RTL8188EU_REG_CR_RXDMA_ENABLE (1u << 3)
#define U80211_DRV_RTL8188EU_REG_CR_PROTOCOL_ENABLE (1u << 4)
#define U80211_DRV_RTL8188EU_REG_CR_SCHEDULE_ENABLE (1u << 5)
#define U80211_DRV_RTL8188EU_REG_CR_MAC_TX_ENABLE (1u << 6)
#define U80211_DRV_RTL8188EU_REG_CR_MAC_RX_ENABLE (1u << 7)
#define U80211_DRV_RTL8188EU_REG_CR_SECURITY_ENABLE (1u << 9)
#define U80211_DRV_RTL8188EU_REG_CR_CALTIMER_ENABLE (1u << 10)

typedef struct {
	uint16_t rtl_id;
	uint8_t mac_address[U80211_DRV_RTL8188EU_MAC_ADDRESS_LEN];
	uint8_t cck_tx_power_base_indexes[U80211_DRV_RTL8188EU_CCK_TX_POWER_BASE_INDEX_COUNT];
	uint8_t ht40_1s_tx_power_base_indexes[U80211_DRV_RTL8188EU_HT40_1S_TX_POWER_BASE_INDEX_COUNT];
	uint8_t xtal_k;
} u80211_drv_rtl8188eu_efuse_t;

typedef struct {
	u80211_drv_device_handle_t device;
	u80211_drv_interface_handle_t interface;
	uint8_t bulk_out_endpoint_count;
	uint8_t tx_endpoint_high;
	uint8_t tx_endpoint_normal;
	uint8_t tx_endpoint_low;
	u80211_drv_rtl8188eu_efuse_t efuse;
} u80211_drv_rtl8188eu_t;

int u80211_drv_rtl8188eu_efuse_prepare(u80211_drv_device_handle_t device);
int u80211_drv_rtl8188eu_efuse_finish(u80211_drv_device_handle_t device);
int u80211_drv_rtl8188eu_read_efuse(u80211_drv_device_handle_t device, uint8_t efuse_map[U80211_DRV_RTL8188EU_EFUSE_MAP_LEN]);
int u80211_drv_rtl8188eu_parse_efuse(const uint8_t efuse_map[U80211_DRV_RTL8188EU_EFUSE_MAP_LEN], u80211_drv_rtl8188eu_efuse_t *result);
int u80211_drv_rtl8188eu_power_active(u80211_drv_device_handle_t device);
int u80211_drv_rtl8188eu_mac_enable_infrastructure(u80211_drv_device_handle_t device);
int u80211_drv_rtl8188eu_reg_read8(u80211_drv_device_handle_t device, uint16_t reg, uint8_t *value);
int u80211_drv_rtl8188eu_reg_read16(u80211_drv_device_handle_t device, uint16_t reg, uint16_t *value);
int u80211_drv_rtl8188eu_reg_read32(u80211_drv_device_handle_t device, uint16_t reg, uint32_t *value);
int u80211_drv_rtl8188eu_reg_write8(u80211_drv_device_handle_t device, uint16_t reg, uint8_t value);
int u80211_drv_rtl8188eu_reg_write16(u80211_drv_device_handle_t device, uint16_t reg, uint16_t value);
int u80211_drv_rtl8188eu_reg_write32(u80211_drv_device_handle_t device, uint16_t reg, uint32_t value);

#endif
