#include <stdint.h>

#include <u80211_drv/rtl8188eu.h>
#include <u80211_drv/status.h>

int u80211_drv_rtl8188eu_mac_enable_infrastructure(u80211_drv_device_handle_t device) {
	// explicitly leave the RX and TX engines disabled.
	uint16_t cr = U80211_DRV_RTL8188EU_REG_CR_HCI_TXDMA_ENABLE |
		U80211_DRV_RTL8188EU_REG_CR_HCI_RXDMA_ENABLE |
		U80211_DRV_RTL8188EU_REG_CR_TXDMA_ENABLE |
		U80211_DRV_RTL8188EU_REG_CR_RXDMA_ENABLE |
		U80211_DRV_RTL8188EU_REG_CR_PROTOCOL_ENABLE |
		U80211_DRV_RTL8188EU_REG_CR_SCHEDULE_ENABLE |
		U80211_DRV_RTL8188EU_REG_CR_SECURITY_ENABLE |
		U80211_DRV_RTL8188EU_REG_CR_CALTIMER_ENABLE;

	return u80211_drv_rtl8188eu_reg_write16(device, U80211_DRV_RTL8188EU_REG_CR, cr);
}

static uint16_t tx_queue_mapping(uint8_t bulk_out_endpoint_count) {
	// map wifi traffic classes onto hardware queues
	uint16_t vi_queue = bulk_out_endpoint_count >= 2 ? U80211_DRV_RTL8188EU_TRXDMA_QUEUE_NORMAL : U80211_DRV_RTL8188EU_TRXDMA_QUEUE_HIGH;
	uint16_t be_bk_queue;

	if (bulk_out_endpoint_count >= 3)
		be_bk_queue = U80211_DRV_RTL8188EU_TRXDMA_QUEUE_LOW;
	else if (bulk_out_endpoint_count == 2)
		be_bk_queue = U80211_DRV_RTL8188EU_TRXDMA_QUEUE_NORMAL;
	else
		be_bk_queue = U80211_DRV_RTL8188EU_TRXDMA_QUEUE_HIGH;

	return (U80211_DRV_RTL8188EU_TRXDMA_QUEUE_HIGH << U80211_DRV_RTL8188EU_REG_TRXDMA_CTRL_VO_SHIFT) |
		(vi_queue << U80211_DRV_RTL8188EU_REG_TRXDMA_CTRL_VI_SHIFT) |
		(be_bk_queue << U80211_DRV_RTL8188EU_REG_TRXDMA_CTRL_BE_SHIFT) |
		(be_bk_queue << U80211_DRV_RTL8188EU_REG_TRXDMA_CTRL_BK_SHIFT) |
		(U80211_DRV_RTL8188EU_TRXDMA_QUEUE_HIGH << U80211_DRV_RTL8188EU_REG_TRXDMA_CTRL_MG_SHIFT) |
		(U80211_DRV_RTL8188EU_TRXDMA_QUEUE_HIGH << U80211_DRV_RTL8188EU_REG_TRXDMA_CTRL_HI_SHIFT);
}

int u80211_drv_rtl8188eu_mac_configure_tx_queues(u80211_drv_device_handle_t device, uint8_t bulk_out_endpoint_count) {
	// configure packet buffer pages
	uint32_t high_pages = U80211_DRV_RTL8188EU_TX_PAGE_NUM_HI_PQ;
	uint32_t normal_pages = bulk_out_endpoint_count >= 2 ? U80211_DRV_RTL8188EU_TX_PAGE_NUM_NORM_PQ : 0;
	uint32_t low_pages = bulk_out_endpoint_count >= 3 ? U80211_DRV_RTL8188EU_TX_PAGE_NUM_LO_PQ : 0;
	uint32_t public_pages = U80211_DRV_RTL8188EU_TX_TOTAL_PAGE_NUM - high_pages - normal_pages - low_pages - 1;

	int status = u80211_drv_rtl8188eu_reg_write32(device, U80211_DRV_RTL8188EU_REG_RQPN_NPQ, normal_pages << U80211_DRV_RTL8188EU_REG_RQPN_NPQ_SHIFT);
	if (status != U80211_DRV_STATUS_SUCCESS)
		return status;

	uint32_t rqpn = U80211_DRV_RTL8188EU_REG_RQPN_LOAD |
		(high_pages << U80211_DRV_RTL8188EU_REG_RQPN_HI_SHIFT) |
		(low_pages << U80211_DRV_RTL8188EU_REG_RQPN_LO_SHIFT) |
		(public_pages << U80211_DRV_RTL8188EU_REG_RQPN_PUB_SHIFT);
	status = u80211_drv_rtl8188eu_reg_write32(device, U80211_DRV_RTL8188EU_REG_RQPN, rqpn);
	if (status != U80211_DRV_STATUS_SUCCESS)
		return status;

	uint16_t trxdma_ctrl;
	status = u80211_drv_rtl8188eu_reg_read16(device, U80211_DRV_RTL8188EU_REG_TRXDMA_CTRL, &trxdma_ctrl);
	if (status != U80211_DRV_STATUS_SUCCESS)
		return status;

	trxdma_ctrl = (trxdma_ctrl & U80211_DRV_RTL8188EU_REG_TRXDMA_CTRL_LOW_CONTROL_MASK) | tx_queue_mapping(bulk_out_endpoint_count);
	return u80211_drv_rtl8188eu_reg_write16(device, U80211_DRV_RTL8188EU_REG_TRXDMA_CTRL, trxdma_ctrl);
}

int u80211_drv_rtl8188eu_mac_configure_rx_fifo_boundary(u80211_drv_device_handle_t device) {
	uint16_t reg = U80211_DRV_RTL8188EU_REG_TRXFF_BNDY + 2;
	int status = u80211_drv_rtl8188eu_reg_write16(device, reg, U80211_DRV_RTL8188EU_RX_FIFO_BOUNDARY);
	if (status != U80211_DRV_STATUS_SUCCESS)
		return status;

	uint16_t boundary;
	status = u80211_drv_rtl8188eu_reg_read16(device, reg, &boundary);
	if (status != U80211_DRV_STATUS_SUCCESS)
		return status;

	if (boundary != U80211_DRV_RTL8188EU_RX_FIFO_BOUNDARY)
		return U80211_DRV_STATUS_FAULTY_HARDWARE;

	return U80211_DRV_STATUS_SUCCESS;
}

int u80211_drv_rtl8188eu_mac_configure_packet_buffer(u80211_drv_device_handle_t device) {
	uint8_t boundary = U80211_DRV_RTL8188EU_TX_TOTAL_PAGE_NUM + 1;
	const uint16_t boundary_regs[] = {
		U80211_DRV_RTL8188EU_REG_TXPKTBUF_BCNQ_BDNY,
		U80211_DRV_RTL8188EU_REG_TXPKTBUF_MGQ_BDNY,
		U80211_DRV_RTL8188EU_REG_TXPKTBUF_WMAC_LBK_BF_HD,
		U80211_DRV_RTL8188EU_REG_TRXFF_BNDY,
		U80211_DRV_RTL8188EU_REG_TDECTRL + 1,
	};

	for (size_t i = 0; i < sizeof(boundary_regs) / sizeof(boundary_regs[0]); ++i) {
		int status = u80211_drv_rtl8188eu_reg_write8(device, boundary_regs[i], boundary);
		if (status != U80211_DRV_STATUS_SUCCESS)
			return status;
	}

	return u80211_drv_rtl8188eu_reg_write8(device, U80211_DRV_RTL8188EU_REG_PBP, U80211_DRV_RTL8188EU_REG_PBP_128_BYTES);
}
