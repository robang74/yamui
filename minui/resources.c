/*
 * Copyright (c) 2007 The Android Open Source Project
 * Copyright (c) 2014 - 2023 Jolla Ltd.
 * Copyright (c) 2023, Roberto A. Foglietta <roberto.foglietta@gmail.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define _DEFAULT_SOURCE

#include <png.h>
#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>

#include <linux/fb.h>
#include <linux/kd.h>

#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/types.h>

#include "minui.h"

#define MSTIME_HEADER_ONLY
#define MSTIME_STATIC_VARS
#include "../get_time_ms.c"

extern char *locale;

#define SURFACE_DATA_ALIGNMENT 8

/* ------------------------------------------------------------------------ */

#define GR_SURFACE_SIZE (sizeof(GRSurface) + SURFACE_DATA_ALIGNMENT)
#define GR_SURFACE_DATA_OFFSET (GR_SURFACE_SIZE - (sizeof(GRSurface) % SURFACE_DATA_ALIGNMENT))

static gr_surface
malloc_surface(size_t data_size)
{
	unsigned char *temp;
	gr_surface surface;

	temp = malloc(GR_SURFACE_SIZE + data_size);
	if (!temp) {
        fprintf(stderr,"ERROR: realloc(*psurface) failed, errno(%d): %s\n",
            errno, strerror(errno));
		return NULL;
    }

	surface = (gr_surface)temp;
	surface->data = temp + GR_SURFACE_DATA_OFFSET;
	return surface;
}

/* ------------------------------------------------------------------------ */

static int
open_png(const char *name, const char *dir, png_structp *png_ptr, png_infop *info_ptr,
	 FILE **fp, png_uint_32 *width, png_uint_32 *height,
	 png_byte *channels)
{
	char resPath[256];
	unsigned char header[8];
	int color_type, bit_depth;
	volatile int result = 0;
	size_t bytesRead;

	snprintf(resPath, sizeof(resPath) - 1, "%s/%s.png", dir, name);
	resPath[sizeof(resPath)-1] = '\0';
	*fp = fopen(resPath, "rb");
	if (*fp == NULL) {
		result = -1;
		goto exit;
	}

	bytesRead = fread(header, 1, sizeof(header), *fp);
	if (bytesRead != sizeof(header)) {
		result = -2;
		goto exit;
	}

	if (png_sig_cmp(header, 0, sizeof(header))) {
		result = -3;
		goto exit;
	}

	*png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL,
					  NULL);
	if (!*png_ptr) {
		result = -4;
		goto exit;
	}

	*info_ptr = png_create_info_struct(*png_ptr);
	if (!*info_ptr) {
		result = -5;
		goto exit;
	}

	if (setjmp(png_jmpbuf(*png_ptr))) {
		result = -6;
		goto exit;
	}

	png_init_io(*png_ptr, *fp);
	png_set_sig_bytes(*png_ptr, sizeof(header));
	png_read_info(*png_ptr, *info_ptr);

	png_get_IHDR(*png_ptr, *info_ptr, width, height, &bit_depth,
		     &color_type, NULL, NULL, NULL);

	*channels = png_get_channels(*png_ptr, *info_ptr);

	if (bit_depth == 8 && *channels == 3 &&
	    color_type == PNG_COLOR_TYPE_RGB) {
		/* 8-bit RGB images: great, nothing to do. */
	} else if (bit_depth <= 8 && *channels == 1 &&
		   color_type == PNG_COLOR_TYPE_GRAY) {
		/* 1-, 2-, 4-, or 8-bit gray images: expand to 8-bit gray. */
		png_set_expand_gray_1_2_4_to_8(*png_ptr);
	} else if (bit_depth <= 8 && *channels == 1 &&
		   color_type == PNG_COLOR_TYPE_PALETTE) {
		/* paletted images: expand to 8-bit RGB. Note that we DON'T
		 * currently expand the tRNS chunk (if any) to an alpha
		 * channel, because minui doesn't support alpha channels
		 * in general. */
		png_set_palette_to_rgb(*png_ptr);
		*channels = 3;
	} else {
		fprintf(stderr,
			"minui doesn't support PNG depth %d channels %d "
			"color_type %d\n",
			bit_depth, *channels, color_type);
		result = -7;
		goto exit;
	}

	return result;

exit:
	if (result < 0)
		png_destroy_read_struct(png_ptr, info_ptr, NULL);

	if (*fp != NULL) {
		fclose(*fp);
		*fp = NULL;
	}

	return result;
}

/* ------------------------------------------------------------------------ */

static void
close_png(png_structp *png_ptr, png_infop *info_ptr, FILE *fp)
{
	png_destroy_read_struct(png_ptr, info_ptr, NULL);
	if (fp != NULL)
		fclose(fp);
}

/* ------------------------------------------------------------------------ */

/* "display" surfaces are transformed into the framebuffer's required
 * pixel format (currently only RGBX is supported) at load time, so
 * gr_blit() can be nothing more than a memcpy() for each row.  The
 * next two functions are the only ones that know anything about the
 * framebuffer pixel format; they need to be modified if the
 * framebuffer format changes (but nothing else should). */

/* Allocate and return a gr_surface sufficient for storing an image of
 * the indicated size in the framebuffer pixel format. */
static gr_surface
init_display_surface(png_uint_32 width, png_uint_32 height)
{
	gr_surface surface;

	if (!(surface = malloc_surface(width * height << 2)))
		return NULL;

	surface->width = width;
	surface->height = height;
	surface->pixel_bytes = 4; //RAF: RGB + Alpha
	surface->row_bytes = width * surface->pixel_bytes;

	return surface;
}

/* ------------------------------------------------------------------------ */

/* Copy 'input_row' to 'output_row', transforming it to the
 * framebuffer pixel format.  The input format depends on the value of
 * 'channels':
 *
 *   1 - input is 8-bit grayscale
 *   3 - input is 24-bit RGB
 *   4 - input is 32-bit RGBA/RGBX
 *
 * 'width' is the number of pixels in the row. */
static void
transform_rgb_to_draw(uint8_t *ip, uint8_t *op, int channels, uint32_t width)
{
	uint_fast32_t x;

	switch (channels) {
	case 1:
		/* expand gray level to RGBX */
		for (x = 0; x < width; x++) {
			*op++ = *ip;
			*op++ = *ip;
			*op++ = *ip;
			*op++ = 0xff;
			ip++;
		}

		break;
	case 3:
		/* expand RGBA to RGBX */
		for (x = 0; x < width; x++) {
			*op++ = *ip++;
			*op++ = *ip++;
			*op++ = *ip++;
			*op++ = 0xff;
		}

		break;
	case 4:
		/* copy RGBA to RGBX */
		memcpy(op, ip, width << 2);
		break;
	}
}

/* ------------------------------------------------------------------------ */

int
res_create_display_surface(const char *name, const char *dir, gr_surface *pSurface)
{
	int result = 0;
	unsigned char *p_row;
	gr_surface surface = NULL;
	png_structp png_ptr = NULL;
	png_infop info_ptr = NULL;
	png_uint_32 width, height;
	png_byte channels;
	FILE *fp = NULL;

	*pSurface = NULL;

	if(!name || !dir) {
		return -1;
	}
	result = open_png(name, dir, &png_ptr, &info_ptr, &fp, &width, &height,
			  &channels);
	if (result < 0)
		return result;

	if (!(surface = init_display_surface(width, height))) {
		result = -8;
		goto exit;
	}

	/* TODO: check for error */
	p_row = malloc(width << 2);
	if(!p_row) {
		fprintf(stderr,"ERROR: realloc(p_row) failed, errno(%d): %s\n",
			errno, strerror(errno));
		result = -9;
		goto exit;
	}

	m_gettimems = -1;
	get_ms_time_run();

	for (uint_fast32_t y = 0; y < height; y++) {
		png_read_row(png_ptr, p_row, NULL);
		transform_rgb_to_draw(p_row, surface->data + y * surface->row_bytes,
            channels, width);
	}

	get_ms_time_run();

	free(p_row);
	p_row = NULL;

	*pSurface = surface;

exit:
	close_png(&png_ptr, &info_ptr, fp);
	if (result < 0 && surface) {
		free(surface);
		*pSurface = NULL;
	}

	return result;
}

/* ------------------------------------------------------------------------ */

int
res_create_multi_display_surface(const char *name, const char *dir, int *frames,
				 gr_surface **pSurface)
{
	unsigned char *p_row;
	int i, result = 0, num_text;
	gr_surface *surface = NULL;
	png_structp png_ptr = NULL;
	png_infop info_ptr = NULL;
	png_uint_32 width, height;
	png_byte channels = 0;
	png_textp text;
	FILE *fp = NULL;

	*pSurface = NULL;
	*frames = -1;

	result = open_png(name, dir, &png_ptr, &info_ptr, &fp, &width, &height,
			  &channels);
	if (result < 0)
		return result;

	*frames = 1;
	if (png_get_text(png_ptr, info_ptr, &text, &num_text)) {
		for (i = 0; i < num_text; i++)
			if (text[i].key && !strcmp(text[i].key, "Frames") &&
			    text[i].text) {
				*frames = atoi(text[i].text);
				break;
			}

		printf("  found frames = %d\n", *frames);
	}

	if (height % *frames != 0) {
		printf("bad height (%ld) for frame count (%d)\n",
		       (long)height, *frames);
		result = -9;
		goto exit;
	}

	if (!(surface = malloc(*frames * sizeof(gr_surface)))) {
		result = -8;
		goto exit;
	}

	for (i = 0; i < *frames; i++) {
		surface[i] = init_display_surface(width, height / *frames);
		if (!surface[i]) {
			result = -8;
			goto exit;
		}
	}

	/* TODO: Check for error */
	p_row = malloc(width << 2);
	for (uint_fast32_t y = 0; y < height; y++) {
		int frame = y % *frames;
		unsigned char *out_row;

		png_read_row(png_ptr, p_row, NULL);
		out_row = surface[frame]->data + (y / *frames) *
			  surface[frame]->row_bytes;
		transform_rgb_to_draw(p_row, out_row, channels, width);
	}

	free(p_row);

	*pSurface = (gr_surface *)surface;

exit:
	close_png(&png_ptr, &info_ptr, fp);

	if (result < 0)
		if (surface) {
			for (i = 0; i < *frames; i++)
				if (surface[i])
					free(surface[i]);

			free(surface);
		}

	return result;
}

/* ------------------------------------------------------------------------ */

int
res_create_alpha_surface(const char *name, const char *dir, gr_surface *pSurface)
{
	int result = 0;
	unsigned char *p_row;
	gr_surface surface = NULL;
	png_structp png_ptr = NULL;
	png_infop info_ptr = NULL;
	png_uint_32 width, height;
	png_byte channels;
	FILE *fp = NULL;

	*pSurface = NULL;

	result = open_png(name, dir, &png_ptr, &info_ptr, &fp, &width, &height,
			  &channels);
	if (result < 0)
		return result;

	if (channels != 1) {
		result = -7;
		goto exit;
	}

	if (!(surface = malloc_surface(width * height))) {
		result = -8;
		goto exit;
	}

	surface->width = width;
	surface->height = height;
	surface->row_bytes = width;
	surface->pixel_bytes = 1;

	for (uint_fast32_t y = 0; y < height; y++) {
		p_row = surface->data + y * surface->row_bytes;
		png_read_row(png_ptr, p_row, NULL);
	}

	*pSurface = surface;
exit:
	close_png(&png_ptr, &info_ptr, fp);
	if (result < 0 && surface != NULL)
		free(surface);

	return result;
}

/* ------------------------------------------------------------------------ */

static int
matches_locale(const char *loc, const char *locale)
{
	int i;

	if (!locale)
		return 0;

	if (!strcmp(loc, locale))
		return 1;

	/* if loc does *not* have an underscore, and it matches the start
	 * of locale, and the next character in locale *is* an underscore,
	 * that's a match.  For instance, loc == "en" matches locale ==
	 * "en_US". */
	for (i = 0; loc[i] != 0 && loc[i] != '_'; i++)
		;

	if (loc[i] == '_')
		return 0;

	return (strncmp(locale, loc, i) == 0 && locale[i] == '_');
}

/* ------------------------------------------------------------------------ */

int
res_create_localized_alpha_surface(const char *name, const char *dir,
                            const char *locale, gr_surface *pSurface)
{
	int result = 0;
	unsigned char *row;
	gr_surface surface = NULL;
	png_structp png_ptr = NULL;
	png_infop info_ptr = NULL;
	png_uint_32 width, height, y;
	png_byte channels;
	FILE *fp = NULL;

	*pSurface = NULL;

	if (!locale) {
		surface = malloc_surface(0);
		surface->width = 0;
		surface->height = 0;
		surface->row_bytes = 0;
		surface->pixel_bytes = 1;
		goto exit;
	}

	result = open_png(name, dir, &png_ptr, &info_ptr, &fp, &width, &height,
			  &channels);
	if (result < 0)
		return result;

	if (channels != 1) {
		result = -7;
		goto exit;
	}

	/* TODO: check for error */
	row = malloc(width);
	for (y = 0; y < height; y++) {
		int h, w;
		char *loc;

		png_read_row(png_ptr, row, NULL);
		w = (row[1] << 8) | row[0];
		h = (row[3] << 8) | row[2];
		loc = (char *)row + 5;

		if (y + 1 + h >= height || matches_locale(loc, locale)) {
			int i;

			printf("  %20s: %s (%d x %d @ %ld)\n", name, loc, w,
			       h, (long)y);

			if (!(surface = malloc_surface(w * h))) {
				result = -8;
				goto exit;
			}

			surface->width = w;
			surface->height = h;
			surface->row_bytes = w;
			surface->pixel_bytes = 1;

			for (i = 0; i < h; i++, y++) {
				png_read_row(png_ptr, row, NULL);
				memcpy(surface->data + i * w, row, w);
			}

			*pSurface = (gr_surface)surface;
			break;
		} else {
			int i;

			for (i = 0; i < h; i++, y++)
				png_read_row(png_ptr, row, NULL);
		}
	}

exit:
	close_png(&png_ptr, &info_ptr, fp);
	if (result < 0 && surface)
		free(surface);

	return result;
}

/* ------------------------------------------------------------------------ */

void
res_free_surface(gr_surface surface)
{
	free(surface);
}
