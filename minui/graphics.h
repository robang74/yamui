/*
 * Copyright (C) 2014 The Android Open Source Project
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

#ifndef _GRAPHICS_H_
#define _GRAPHICS_H_

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <stdbool.h>
#include <stdint.h>

#include "minui.h"

#define ABSOLUTE_DISPLAY_MARGIN_X 20
#define ABSOLUTE_DISPLAY_MARGIN_Y 20

typedef uint32_t w32;
#define comp_to_rgba(r,g,b,a) ((w32)(r) | (w32)(g) << 8 | (w32)(b) << 16 | (a) << 24)
#define gr_update_rgba() comp_to_rgba(gr_current_r, gr_current_g, gr_current_b, gr_current_a)

typedef struct minui_backend {
	/* Initializes the backend and returns a gr_surface to draw into. */
	gr_surface (*init)(struct minui_backend *backend, bool blank);

	/* Causes the current drawing surface (returned by the most recent
	 * call to flip() or init()) to be displayed, and returns a new
	 * drawing surface. */
	gr_surface (*flip)(struct minui_backend *backend);

	/* Blank (or unblank) the screen. */
	void (*blank)(struct minui_backend *backend, bool blank);

	/* Device cleanup when drawing is done. */
	void (*exit)(struct minui_backend *backend);

	/* Save screen content to internal buffer. */
	void (*save)(struct minui_backend *backend);

	/* Restore screen content from internal buffer. */
	void (*restore)(struct minui_backend *backend);
} minui_backend;

minui_backend *open_fbdev(void);
minui_backend *open_adf(void);
minui_backend *open_drm(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _GRAPHICS_H_ */
