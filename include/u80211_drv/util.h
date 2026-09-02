#ifndef U80211_DRV_UTIL_H
#define U80211_DRV_UTIL_H

#include <stdint.h>

static inline uint16_t u80211_drv_host_to_le16(uint16_t value) {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
	return value;
#else
	return __builtin_bswap16(value);
#endif
}

static inline uint32_t u80211_drv_host_to_le32(uint32_t value) {
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
	return value;
#else
	return __builtin_bswap32(value);
#endif
}

static inline uint16_t u80211_drv_le_to_host16(uint16_t value) {
	return u80211_drv_host_to_le16(value);
}

static inline uint32_t u80211_drv_le_to_host32(uint32_t value) {
	return u80211_drv_host_to_le32(value);
}

#endif
