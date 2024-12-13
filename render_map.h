#ifndef RENDER_MAP
#define RENDER_MAP

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

bool map_render(void** out, size_t* out_len, uint8_t* data, size_t data_len,
				bool isometric);

#endif
