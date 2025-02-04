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

#include <time.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/ioctl.h>

#include <linux/fb.h>
#include <linux/kd.h>

#include "font_10x18.h"
#include "minui.h"
#include "graphics.h"

#define MSTIME_HEADER_ONLY
#define MSTIME_STATIC_VARS
#include "../get_time_ms.c"

typedef struct {
	GRSurface *texture;
	int cwidth;
	int cheight;
} GRFont;

static GRFont *gr_font = NULL;
static minui_backend *gr_backend = NULL;

static int overscan_percent  = OVERSCAN_PERCENT;
static int overscan_offset_x = 0;
static int overscan_offset_y = 0;

static int gr_vt_fd = -1;

static uint8_t gr_current_r;
static uint8_t gr_current_g;
static uint8_t gr_current_b;
static uint8_t gr_current_a;
static uint32_t gr_current_rgba;

static GRSurface *gr_draw = NULL;

extern long long int v_shift;

/* ------------------------------------------------------------------------ */

static bool
outside(int x, int y)
{
	return x < 0 || x >= gr_draw->width || y < 0 || y >= gr_draw->height;
}

/* ------------------------------------------------------------------------ */

int gr_measure(const char *s)
{
    return gr_font->cwidth * strlen(s);
}

/* ------------------------------------------------------------------------ */

void gr_font_size(int *x, int *y)
{
    *x = gr_font->cwidth;
    *y = gr_font->cheight;
}

/* ------------------------------------------------------------------------ */

//RAF: integer divisions requires to be rounded to the nearest integer value
//     but adding 127 makes the unsigned char overflow therefore (unsigned)

#define alpha_apply(sx, bg, a) (unsigned char)( ( (unsigned)127 + \
	((unsigned)sx * (255 - a)) + ((unsigned)bg * a) ) / 255 )

static void
char_blend(uint8_t *sx, uint32_t src_row_bytes, uint8_t *px, uint8_t *bx,
    uint32_t dst_row_bytes, uint32_t width, uint32_t height, uint32_t factor)
{
	uint8_t a;
	uint_fast32_t i, j, l, k, z = 0;
    uint32_t *wbx = (uint32_t *)bx, *wpx = (uint32_t *)px;

/* RAF: in the most generic case the RGBA is not the only format possible.
 *      The pixel_bytes should be passed as function parameter and verified.
 *      When it is less than 3, this function cannot deal with it returns.
 *      When it is 3, then the alpha layer can be ignored and the RGB set.
 */

    for (j = 0; j < height; j++)
    {
        for (l = 0; l < factor; l++)
        {
            for (i = 0; i < width; i++)
            {
                if (gr_current_a < 255)
                    a = alpha_apply(0, gr_current_a, sx[i]);
                else
                    a = sx[i];

                for (z = i * factor, k = 0; k < factor; k++, z++)
                {
#if 0
                    printf("a: %u, j:%d/%d, l:%d, i:%d/%d, k:%d, r:%d/%d, f: %d\n",
                        a, j, height, l, i, width, k, src_row_bytes, dst_row_bytes,
                        factor); fflush(stdout);
#endif
                    if (a == 255)
                    {
                        //RAF: transparency none
                        wpx[z] = gr_current_rgba;
                    } else
                    if (a > 0)
                    {
                        int h = z << 2;
                        //RAF: transparency dims
                        px[h+0] = alpha_apply(px[h+0], gr_current_r, a);
                        px[h+1] = alpha_apply(px[h+1], gr_current_g, a);
                        px[h+2] = alpha_apply(px[h+2], gr_current_b, a);
                    }
                    if(wbx) wbx[z] = wpx[z];
                }
            }
            px += dst_row_bytes;
            wpx = (uint32_t *)px;
            if(bx) {
                bx += dst_row_bytes;
                wbx = (uint32_t *)bx;
            }
        }
        sx += src_row_bytes;
    }
}

/* ------------------------------------------------------------------------ */

// RAF: (x,y) is the coordinates at which it starts to render the text
//      the following macro can be useful somewhere else (TODO)

#define gr_draw_data_ptr(x,y) (uint8_t *)(gr_draw_ptr->data + \
        (x * gr_draw_ptr->pixel_bytes)) + (y * gr_draw_ptr->row_bytes)

#define gr_flip_data_ptr(x,y) (uint8_t *)(gr_flip_ptr->data + \
        (x * gr_flip_ptr->pixel_bytes)) + (y * gr_flip_ptr->row_bytes)

/*
#ifndef _GET_TIME_MS_H_
#define MIL (1000ULL)
#define INT_DIV(a, b) ( typeof(a)(a + (b>>1)) / b )
#define INT_RMN(a, b) ( typeof(a)(a % b) )
#define MIL_DIV(a) INT_DIV(a, MIL)
#define MIL_RMN(a) INT_DIV(a, MIL)
#endif
*/

void
gr_text(int kx, int ky, const char *s, int bold, int factor, int row)
{
	GRFont *font = gr_font;
	int off, frch, frcw, x, y, strw = 0;

	if (!font->texture)
		return;

	if (gr_current_a == 0)
		return;

    frcw = font->cwidth  * factor;
    frch = font->cheight * factor;

	bold = bold && (font->texture->height != font->cheight);
	
	if(kx < 0) {
	    //RAF: align the text on the right side
	    kx = -kx; 
	} else {
	    //RAF: center the text on the screen
	    strw = (frcw * strlen(s)) >> 1;
    }

    x = MIL_DIV(gr_draw->width  * kx) + overscan_offset_x - strw;
    if(x < ABSOLUTE_DISPLAY_MARGIN_X) x = ABSOLUTE_DISPLAY_MARGIN_X;

    y = MIL_DIV(gr_draw->height * ky) + overscan_offset_y + v_shift;
    if(y < ABSOLUTE_DISPLAY_MARGIN_Y) y = ABSOLUTE_DISPLAY_MARGIN_Y;
        
    y += (row * frch) - MIL_DIV(frch * ky); //RAF: progressive vertical shift

	printf("gr_text -> mpl: %d, fnt: %d.%d, off: %d.%d, kxy: %d.%d, pos: %d.%d\n",
	    factor, font->cwidth, font->cheight, overscan_offset_x,
	    overscan_offset_y, kx, ky, x, y);

	m_gettimems = -1;
	get_ms_time_run();

    static GRSurface *gr_draw_ptr = NULL, *gr_flip_ptr = NULL;
#if 1
    if(!gr_flip_ptr || !gr_draw_ptr) {
        gr_flip_ptr = gr_flip_n_copy();
        gr_draw_ptr = gr_draw;
    }
#else
    if(!gr_flip_ptr || !gr_draw_ptr) {
        gr_save();
        gr_flip_ptr = gr_flip();
        gr_restore();
        gr_draw_ptr = gr_draw;
    }
#endif

	get_ms_time_run();

	while ((off = *s++)) {
		if (outside(x + frcw - 1, y + frch - 1))
			break;

		if ((off -= 32) >= 0 && off < 96) {
			uint8_t *src_p = font->texture->data + (off * font->cwidth) +
				(bold ? font->cheight * font->texture->row_bytes : 0);

			char_blend(src_p, font->texture->row_bytes, gr_draw_data_ptr(x, y),
				   gr_flip_data_ptr(x,y), gr_draw_ptr->row_bytes, font->cwidth,
				   font->cheight, factor);
		}
		x += frcw;
	}
}

/* ------------------------------------------------------------------------ */

void
gr_texticon(int x, int y, GRSurface *icon)
{
	unsigned char *src_p, *dst_p;

	if (!icon)
		return;

	if (icon->pixel_bytes != 1) {
		printf("gr_texticon: source has wrong format\n");
		return;
	}

	x += overscan_offset_x;
	y += overscan_offset_y;

	if (outside(x, y) ||
	    outside(x + icon->width - 1, y + icon->height - 1))
		return;

	src_p = icon->data;
	dst_p = gr_draw->data + y * gr_draw->row_bytes +
				x * gr_draw->pixel_bytes;

	char_blend(src_p, icon->row_bytes, dst_p, NULL, gr_draw->row_bytes,
		   icon->width, icon->height, 1);
}

/* ------------------------------------------------------------------------ */

void
gr_color(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
	gr_current_r = r;
	gr_current_g = g;
	gr_current_b = b;
	gr_current_a = a;
	gr_current_rgba = gr_update_rgba();
}

/* ------------------------------------------------------------------------ */

void
gr_clear(void)
{
	if (gr_current_r == gr_current_g && gr_current_r == gr_current_b)
		memset(gr_draw->data, gr_current_r,
		       gr_draw->height * gr_draw->row_bytes);
	else {
		int32_t x, y;
		uint8_t *px = gr_draw->data;

		for (y = 0; y < gr_draw->height; y++) {
			for (x = 0; x < gr_draw->width; x++) {
				*px++ = gr_current_r;
				*px++ = gr_current_g;
				*px++ = gr_current_b;
				px++;
			}

			px += gr_draw->row_bytes -
			      (gr_draw->width * gr_draw->pixel_bytes);
		}
	}
}

/* ------------------------------------------------------------------------ */

void
gr_fill(int x1, int y1, int x2, int y2)
{
	uint8_t *p;

	x1 += overscan_offset_x;
	y1 += overscan_offset_y;

	x2 += overscan_offset_x;
	y2 += overscan_offset_y;

	if (outside(x1, y1) || outside(x2 - 1, y2 - 1))
		return;

	p = gr_draw->data + y1 * gr_draw->row_bytes +
	    x1 * gr_draw->pixel_bytes;

	if (gr_current_a == 255) {
		int x, y;

		for (y = y1; y < y2; y++) {
			uint8_t *px = p;

			for (x = x1; x < x2; x++) {
				*px++ = gr_current_r;
				*px++ = gr_current_g;
				*px++ = gr_current_b;
				px++;
			}

			p += gr_draw->row_bytes;
		}
	} else if (gr_current_a > 0) {
		int x, y;

		for (y = y1; y < y2; y++) {
			uint8_t *px = p;

			for (x = x1; x < x2; x++) {
				*px = (*px * (255 - gr_current_a) +
				       gr_current_r * gr_current_a) / 255;
				px++;
				*px = (*px * (255 - gr_current_a) +
				       gr_current_g * gr_current_a) / 255;
				px++;
				*px = (*px * (255 - gr_current_a) +
				       gr_current_b * gr_current_a) / 255;
				px++;
				px++;
			}

			p += gr_draw->row_bytes;
		}
	}
}

/* ------------------------------------------------------------------------ */

void
gr_blit(GRSurface *source, int sx, int sy, int w, int h, int dx, int dy)
{
	int i;
	uint8_t *src_p, *dst_p;

	if (!source)
		return;

	if (gr_draw->pixel_bytes != source->pixel_bytes) {
		printf("gr_blit: source has wrong format\n");
		return;
	}

	dx += overscan_offset_x;
	dy += overscan_offset_y;

	if (dx < 0) sx -= dx, w += dx, dx = 0;
	if (dy < 0) sy -= dy, h += dy, dy = 0;
	if (dx + w > gr_draw->width) w = gr_draw->width - dx;
	if (dy + h > gr_draw->height) h = gr_draw->height - dy;
	if (w <= 0 || h <= 0)
		return;

	src_p = source->data + sy * source->row_bytes +
			       sx * source->pixel_bytes;
	dst_p = gr_draw->data + dy * gr_draw->row_bytes +
				dx * gr_draw->pixel_bytes;

	for (i = 0; i < h; i++) {
		memcpy(dst_p, src_p, w * source->pixel_bytes);
		src_p += source->row_bytes;
		dst_p += gr_draw->row_bytes;
	}
}

/* ------------------------------------------------------------------------ */

unsigned int
gr_get_width(GRSurface *surface)
{
	if (!surface)
		return 0;

	return surface->width;
}

/* ------------------------------------------------------------------------ */

unsigned int
gr_get_height(GRSurface *surface)
{
	if (!surface)
		return 0;

	return surface->height;
}

/* ------------------------------------------------------------------------ */

static void
gr_init_font(void)
{
	int res;
	static const char font_path[] = "/res/images/font.png";

	/* TODO: Check for error */
	gr_font = calloc(sizeof(*gr_font), 1);

	bool font_loaded = false;

	if (access(font_path, F_OK) == -1 && errno == ENOENT) {
		/* Not having a font file is normal, no need
		 * to complain. */
	}
	else if (!(res = res_create_alpha_surface(font_path, NULL, &gr_font->texture))) {
		/* The font image should be a 96x2 array of character images.
		 * The columns are the printable ASCII characters 0x20 - 0x7f.
		 * The top row is regular text; the bottom row is bold. */
		gr_font->cwidth = gr_font->texture->width / 96;
		gr_font->cheight = gr_font->texture->height / 2;
		font_loaded = true;
	}
	else {
		printf("%s: failed to read font: res=%d\n", font_path, res);
	}

	if (!font_loaded) {
		uint8_t *bits, data, *in = font.rundata;

		/* fall back to the compiled-in font. */
		/* TODO: Check for error */
		gr_font->texture = malloc(sizeof(*gr_font->texture));
		gr_font->texture->width = font.width;
		gr_font->texture->height = font.height;
		gr_font->texture->row_bytes = font.width;
		gr_font->texture->pixel_bytes = 1;

		/* TODO: Check for error */
		bits = malloc(font.width * font.height);
		gr_font->texture->data = (void *)bits;

		while ((data = *in++)) {
			memset(bits, (data & 0x80) ? 255 : 0, data & 0x7f);
			bits += (data & 0x7f);
		}

		gr_font->cwidth = font.cwidth;
		gr_font->cheight = font.cheight;
	}
}

/* ------------------------------------------------------------------------ */

GRSurface *gr_flip(void)
{
    GRSurface *srf_ptr = gr_draw;
    gr_draw = gr_backend->flip(gr_backend);
    return srf_ptr;
}

GRSurface *gr_flip_n_copy(void)
{
    GRSurface *gr_flip_ptr;

    gr_save();
    gr_flip_ptr = gr_flip();
    gr_restore();

	return gr_flip_ptr;
}

/* ------------------------------------------------------------------------ */

int gr_init(bool blank)
{
	if ((gr_vt_fd = open("/dev/tty0", O_RDWR | O_SYNC)) < 0) {
		/* This is non-fatal; post-Cupcake kernels don't have tty0. */
		perror("can't open /dev/tty0");
	} else if (ioctl(gr_vt_fd, KDSETMODE, (void *)KD_GRAPHICS)) {
		/* However, if we do open tty0, we expect the ioctl
		 * to work. */
		perror("failed KDSETMODE to KD_GRAPHICS on tty0");
        goto err_quit;
	}

#if 0
    if(!gr_backend)
	    gr_backend = open_adf();
#endif

	if(!gr_backend)
	    gr_backend = open_drm();
	if(!gr_backend)
	    gr_backend = open_fbdev();
	if(!gr_backend)
        goto err_quit;

    m_gettimems = -1;
	get_ms_time_run();
	
	gr_draw = gr_backend->init(gr_backend, blank);
	if (!gr_draw) {
		gr_backend->exit(gr_backend);
        goto err_quit;
	}

	get_ms_time_run();

#if 0
	gr_flip();
	if (!gr_draw)
		return -1;
	gr_flip();
	if (!gr_draw)
		return -1;
	get_ms_time_run();
#endif

	gr_init_font();

    if(overscan_percent) {
	    overscan_offset_x = INT_DIV(gr_draw->width  * overscan_percent, 100);
	    overscan_offset_y = INT_DIV(gr_draw->height * overscan_percent, 100);
	}

	return 0;

err_quit:
	if(gr_vt_fd > -1)
	    close(gr_vt_fd);
	gr_vt_fd = -1;
    return -1;
}

/* ------------------------------------------------------------------------ */

void
gr_exit(void)
{
	gr_backend->exit(gr_backend);

	ioctl(gr_vt_fd, KDSETMODE, (void *)KD_TEXT);
	close(gr_vt_fd);
	gr_vt_fd = -1;
}

/* ------------------------------------------------------------------------ */

int
gr_fb_width(void)
{
	return gr_draw->width - 2 * overscan_offset_x;
}

/* ------------------------------------------------------------------------ */

int
gr_fb_height(void)
{
	return gr_draw->height - 2 * overscan_offset_y;
}

/* ------------------------------------------------------------------------ */

void
gr_fb_blank(bool blank)
{
	gr_backend->blank(gr_backend, blank);
}

/* ------------------------------------------------------------------------ */

/* Save screen content to internal buffer. */
void
gr_save(void)
{
	if (gr_backend->save)
		gr_backend->save(gr_backend);
}

/* ------------------------------------------------------------------------ */

/* Restore screen content from internal buffer. */
void
gr_restore(void)
{
	if (gr_backend->restore)
		gr_backend->restore(gr_backend);
}
