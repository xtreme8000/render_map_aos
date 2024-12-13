#include <stdio.h>
#include <stdlib.h>

#include "render_map.h"

void* read_file(const char* filename, size_t* length) {
	if(!filename || !length)
		return NULL;

	FILE* f = fopen(filename, "rb");

	if(!f)
		return NULL;

	fseek(f, 0, SEEK_END);
	*length = ftell(f);
	void* res = malloc(*length);

	if(!res) {
		fclose(f);
		return NULL;
	}

	fseek(f, 0, SEEK_SET);

	if(fread(res, *length, 1, f) != 1) {
		fclose(f);
		return NULL;
	}

	fclose(f);
	return res;
}

bool write_file(const char* filename, void* data, size_t length) {
	if(!filename || !data)
		return false;

	FILE* f = fopen(filename, "wb");

	if(!f)
		return false;

	if(fwrite(data, length, 1, f) != 1) {
		fclose(f);
		return false;
	}

	fclose(f);
	return true;
}

int main(int argc, char** argv) {
	if(argc < 3) {
		printf("Usage: ./render_map map.vxl map.png [0/1]\n");
		return 1;
	}

	size_t input_length;
	void* input = read_file(argv[1], &input_length);

	if(!input) {
		printf("Could not read file %s\n", argv[1]);
		return 1;
	}

	bool isometric = true;

	if(argc > 3)
		isometric = atoi(argv[3]);

	void* output;
	size_t output_length;
	if(!map_render(&output, &output_length, input, input_length, isometric)) {
		printf("Could not render map\n");
		return 1;
	}

	if(!write_file(argv[2], output, output_length)) {
		printf("Could not write file %s\n", argv[2]);
		return 1;
	}

	return 0;
}