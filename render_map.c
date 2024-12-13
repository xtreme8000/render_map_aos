#include <stdlib.h>

#include "libvxl.h"
#include "lodepng.h"
#include "render_map.h"

#define SHADOW_DISTANCE 18
#define SHADOW_STEP 2

#define rgba(r, g, b, a)                                                       \
	(((int)(a) << 24) | ((int)(b) << 16) | ((int)(g) << 8) | (int)(r))
#define red(col) (((col) >> 16) & 0xFF)
#define green(col) (((col) >> 8) & 0xFF)
#define blue(col) ((col) & 0xFF)

#define ADVANCE_X 5
#define ADVANCE_Y 2
#define ADVANCE_Z 6
#define STENCIL_SIZE 11

static uint8_t stencil[STENCIL_SIZE * STENCIL_SIZE] = {
	0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xbf, 0xbf, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x9f, 0x9f,
	0xbf, 0xbf, 0xbf, 0xbf, 0xff, 0xff, 0xff, 0x9f, 0x9f, 0x9f, 0x9f,
	0xbf, 0xbf, 0xbf, 0xbf, 0xbf, 0xbf, 0x9f, 0x9f, 0x9f, 0x9f, 0x9f,
	0xbf, 0xbf, 0xbf, 0xbf, 0xbf, 0xbf, 0x9f, 0x9f, 0x9f, 0x9f, 0x9f,
	0xbf, 0xbf, 0xbf, 0xbf, 0xbf, 0xbf, 0x9f, 0x9f, 0x9f, 0x9f, 0x9f,
	0xbf, 0xbf, 0xbf, 0xbf, 0xbf, 0xbf, 0x9f, 0x9f, 0x9f, 0x9f, 0x9f,
	0x00, 0x00, 0xbf, 0xbf, 0xbf, 0xbf, 0x9f, 0x9f, 0x9f, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0xbf, 0xbf, 0x9f, 0x00, 0x00, 0x00, 0x00,
};

static bool vxl_visible(struct libvxl_map* map, int x, int y, int z) {
	return !libvxl_map_issolid(map, x, y, z - 1)
		|| !libvxl_map_issolid(map, x - 1, y, z)
		|| !libvxl_map_issolid(map, x, y + 1, z);
}

static int sunblock(struct libvxl_map* map, int x, int y, int z) {
	int dec = SHADOW_DISTANCE;
	int i = 127;

	while(dec && z) {
		y = (y == 0) ? map->height - 1 : y - 1;

		if(libvxl_map_issolid(map, x, y, --z))
			i -= dec;

		dec -= SHADOW_STEP;
	}

	return i;
}

bool map_render(void** out, size_t* out_len, uint8_t* data, size_t data_len,
				bool isometric) {
	size_t map_size;
	size_t map_depth;

	if(!libvxl_size(&map_size, &map_depth, data, data_len)) {
		map_size = 512;
		map_depth = 64;
	}

	struct libvxl_map map;
	if(!libvxl_create(&map, map_size, map_size, map_depth, data, data_len))
		return false;

	size_t width, height;

	if(isometric) {
		width = map_size * 2 * ADVANCE_X + 1;
		height = ((map_size - 1) * 2) * ADVANCE_Y + (map_depth - 1) * ADVANCE_Z
			+ STENCIL_SIZE;
	} else {
		width = height = map_size;
	}

	uint8_t* image = calloc(width * height, sizeof(uint8_t) * 4);

	if(!image) {
		libvxl_free(&map);
		return false;
	}

	if(isometric) {
		for(int x = map_size - 1; x >= 0; x--) {
			for(size_t y = 0; y < map_size; y++) {
				for(size_t z = 0; z < map_depth; z++) {
					if(libvxl_map_issolid(&map, x, y, (map_depth - 1) - z)
					   && (vxl_visible(&map, x, y, (map_depth - 1) - z)
						   || x == 0 || y == map_size - 1)) {
						int shade = sunblock(&map, x, y, (map_depth - 1) - z);
						uint32_t col
							= libvxl_map_get(&map, x, y, (map_depth - 1) - z);
						int cr = red(col) * shade / 127;
						int cg = green(col) * shade / 127;
						int cb = blue(col) * shade / 127;

						int bx = (x + y) * ADVANCE_X;
						int by = (y - x + map_size - 1) * ADVANCE_Y
							+ ((map_depth - 1) - z) * ADVANCE_Z;

						uint8_t* gray = stencil;
						for(size_t py = 0; py < STENCIL_SIZE; py++) {
							for(size_t px = 0; px < STENCIL_SIZE; px++) {
								uint8_t t = *(gray++);

								if(t > 0) {
									size_t pos
										= (bx + px + (by + py) * width) * 4;
									image[pos + 0] = cr * t / 255;
									image[pos + 1] = cg * t / 255;
									image[pos + 2] = cb * t / 255;
									image[pos + 3] = 0xFF;
								}
							}
						}
					}
				}
			}
		}
	} else {
		for(size_t y = 0; y < map_size; y++) {
			for(size_t x = 0; x < map_size; x++) {
				uint32_t res[2];
				libvxl_map_gettop(&map, x, y, res);
				int shade = sunblock(&map, x, y, res[1]);

				size_t pos = (x + y * width) * 4;
				image[pos + 0] = red(res[0]) * shade / 127;
				image[pos + 1] = green(res[0]) * shade / 127;
				image[pos + 2] = blue(res[0]) * shade / 127;
				image[pos + 3] = 0xFF;
			}
		}
	}

	libvxl_free(&map);

	if(lodepng_encode_memory((unsigned char**)out, out_len, image, width,
							 height, LCT_RGBA, 8)
	   != 0) {
		free(image);
		return false;
	}

	free(image);
	return true;
}
