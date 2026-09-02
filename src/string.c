#include <stddef.h>
#include <stdint.h>

#include <u80211_drv/string.h>

void *u80211_drv_memset(void *destination, int value, size_t size) {
	uint8_t *destination_bytes = destination;

	for (size_t i = 0; i < size; ++i)
		destination_bytes[i] = (uint8_t)value;

	return destination;
}
