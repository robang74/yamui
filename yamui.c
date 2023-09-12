/*
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

#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>
#include <unistd.h>

#include <signal.h>
#include <sys/signalfd.h>
#include <sys/select.h>

#include "os-update.h"
#include "minui/minui.h"

#define MSTIME_HEADER_ONLY
#define MSTIME_STATIC_VARS
#include "get_time_ms.c"

#define IMAGES_MAX	32
#define TXTRWS_MAX  32

static struct option options[] = {
	{"animate",     required_argument, 0, 'a'},
	{"imagesdir",   required_argument, 0, 'i'},
	{"progressbar", required_argument, 0, 'p'},
	{"stopafter",   required_argument, 0, 's'},
	{"text",        required_argument, 0, 't'},
	{"fontmultipl", required_argument, 0, 'm'},
	{"xpos",        required_argument, 0, 'x'},
	{"ypos",        required_argument, 0, 'x'},
	{"cleanup",     no_argument,       0, 'k'},
	{"help",        no_argument,       0, 'h'},
	{0, 0, 0, 0},
};

static bool do_cleanup = false;
static long long int app_font_multipl = 0;
static long long int app_text_xpos = 0, app_text_ypos = 0;

long long int v_shift = 0;

/* ------------------------------------------------------------------------ */

static int __attribute__((unused))
_wait_signalfd(int sigfd, unsigned long long int msecs)
{
	int ret;
	fd_set fdset;
	struct timespec ts = {
		.tv_sec = msecs / 1000,
		.tv_nsec = (msecs % 1000) * 1000000
	};

	FD_ZERO(&fdset);
	if (sigfd >= 0)
		FD_SET(sigfd, &fdset);

	ret = pselect(sigfd + 1, &fdset, NULL, NULL, msecs ? &ts : NULL, NULL);
	if (ret > 0)
		printf("Interrupted, bailing out\n");
	else if (ret == -1)
		printf("An error occured, bailing out\n");
	return ret;
}

#include <errno.h>
#include <unistd.h>
#include <string.h>

static int
wait_signalfd(int sigfd, unsigned long long int msecs)
{
    if(!msecs)
        return _wait_signalfd(sigfd, msecs);

    int ret = usleep(msecs * 1000);
    if (ret > 0)
        printf("Interrupted, bailing out\n");
    else if (ret < 0)
        printf("An error occured, errno(%d): %s\n", errno, strerror(errno));

    return ret;
}

/* ------------------------------------------------------------------------ */

#define basename (argv_ptr[get_my_basename_index()])

static char **argv_ptr = NULL;

static inline int
get_my_basename_index(void)
{
    static int i, n = -1;
    if(!argv_ptr)
        return -1;
    if (n >= 0)
        return n;
    char *path = argv_ptr[0];
    for(i = 0; path[i]; i++)
        if(path[i] == '/')
            n = i;
    return ++n;
}

static void
short_help(void)
{
	printf("\n  USAGE: %s [OPTIONS] [IMAGE(s)]\n\n", basename);
}

/* ------------------------------------------------------------------------ */

static void
print_help(void)
{
	printf("\n");
	printf("  yamui - tool to display progress bar, logo, or small animation on UI\n");
	short_help();
	printf("    DIR        - the folder path in which the images are searched or\n");
	printf("                 by default /res/images\n");
	printf("    IMAGE(s)   - images in PNG format with .png extention which file\n");
	printf("                 names can be found in DIR without the .png extension.\n");
	printf("                 The maximum of %d pictures is supported.\n", IMAGES_MAX);
	printf("    STRING(s)  - text strings composed by printable chars, %d max rows\n", TXTRWS_MAX);
	printf("\n");
	printf("    OPTIONS:\n");
	printf("\n");
	printf("  --animate=PERIOD, -a PERIOD\n");
	printf("         Show IMAGEs (at least 2) in rotation over PERIOD ms\n");
	printf("  --imagesdir=DIR, -i DIR\n");
	printf("         Load IMAGE(s) from DIR, /res/images by default\n");
	printf("  --progressbar=TIME, -p TIME\n");
	printf("         Show a progess bar over TIME milliseconds\n");
	printf("  --stopafter=TIME, -s TIME\n");
	printf("         Stop showing the IMAGE(s) after TIME milliseconds\n");
	printf("  --text=STRING, -t STRING\n");
	printf("         Show STRING on the screen, multiple times for each row\n");
	printf("  --fontmultipl=FACTOR, -m FACTOR\n");
	printf("         Increase the font size by a factor between 1 and 16\n");
	printf("  --xpos=THOUSANDTHS, -x THOUSANDTHS\n");
	printf("         Set the text horizontal center to x/1000 of the screen width\n");
	printf("  --ypos=THOUSANDTHS, -y THOUSANDTHS\n");
	printf("         Set the text vertical origin to y/1000 of the screen height\n");
	printf("  --vshift=THOUSANDTHS, -v THOUSANDTHS\n");
	printf("         Set the vertical shift to v/1000 of the screen height\n");
	printf("  --cleanup, -k\n");
	printf("         Exit closing and freeing resources but the kernel does it\n");
	printf("  --help, -h\n");
	printf("         Print this help\n");
	printf("\n");
}

/* ------------------------------------------------------------------------ */

/* Add text to both sides of the "flip" */
static void
add_text(char **text, int count)
{
	if (!text || !count)
		return;

	gr_color(255, 255, 255, 255);

	for(int i = 0; i < count; i++)
	    gr_text(app_text_xpos, app_text_ypos, text[i], 1, app_font_multipl, i);

//	gr_copy();
	gr_flip();
}

/* ------------------------------------------------------------------------ */

int
main(int argc, char *argv[])
{
	int c, option_index;
	unsigned long int animate_ms = 0;
	unsigned long long int stop_ms = 0;
	unsigned long long int progress_ms = 0;
	char * text[512];
	char * images[IMAGES_MAX];
	char * images_dir = "/res/images";
	int image_count = 0, text_count = 0;
	int ret = 0;
	int i = 0;
	int sigfd = -1;
	sigset_t mask;

	argv_ptr = argv;

	get_ms_time_run();

	setlinebuf(stdout);

	while (1) {
		c = getopt_long(argc, argv, "a:i:p:s:t:m:x:y:v:kh", options,
				&option_index);
		if (c == -1)
			break;

		switch (c) {
		case 'a':
			printf("got animate %s ms\n", optarg);
			animate_ms = strtoul(optarg, (char **)NULL, 10);
			break;
		case 'k':
			printf("clean up resources\n");
			do_cleanup = true;
			break;
		case 'i':
			printf("got imagesdir \"%s\"\n", optarg);
			images_dir = optarg;
			break;
		case 'p':
			printf("got progressbar %s ms\n", optarg);
			progress_ms = strtoull(optarg, (char **)NULL, 10);
			break;
		case 's':
			printf("got stop at %s ms\n", optarg);
			stop_ms = strtoull(optarg, (char **)NULL, 10);
			break;
		case 't':
			printf("got text[%d] '%s' to display\n", text_count, optarg);
            if (!app_font_multipl)
                app_font_multipl = 1;
            else
            if (app_font_multipl > 16) {
                printf("The font multiplier is out of range");
                app_font_multipl = 16;
            }
			text[text_count++] = optarg;
			break;
        case 'm':
            printf("got font %s multipier\n", optarg);
            app_font_multipl = strtoull(optarg, NULL, 10);
            break;
        case 'x':
            printf("got text x-pos: %s/1000\n", optarg);
            app_text_xpos = strtoull(optarg, NULL, 10);
            break;
        case 'y':
            printf("got text y-pos: %s/1000\n", optarg);
            app_text_ypos = strtoull(optarg, NULL, 10);
            break;
        case 'v':
            printf("got v-shift: %s/1000\n", optarg);
            v_shift = strtoll(optarg, NULL, 10);
            break;            
		case 'h':
			print_help();
			goto out;
			break;
		default:
			printf("getopt option '-%c' unrecognised, ignored\n", c);
			break;
		}
	}

	while (optind < argc && image_count < IMAGES_MAX)
		images[image_count++] = argv[optind++];
	printf("got %d image(s) to display\n", image_count);

	get_ms_time_run();

	if (osUpdateScreenInit())
		return -1;

    if(v_shift) {
        v_shift = MIL_DIV(v_shift * gr_fb_height());
        printf("real v-shift is %lld pixels\n", v_shift);
    }

    get_ms_time_run(); //RAF: 0.366s are spent in initialisation

	/* Allow SIGTERM and SIGINT to interrupt pselect() and move to cleanup */
	sigemptyset(&mask);
	sigaddset(&mask, SIGINT);
    sigaddset(&mask, SIGTERM);
	sigfd = signalfd(-1, &mask, 0);
	if (sigfd == -1) {
		printf("Could not create signal fd\n");
		goto cleanup; //RAF, TODO: it can be return -1 here
	}
	if (sigprocmask(SIG_BLOCK, &mask, NULL) == -1) {
		printf("Could not block signals\n");
		goto cleanup; //RAF, TODO: it can be return -1 here
	}

	get_ms_time_run();

	/* In case there is text to add, add it to both sides of the "flip" */
	if(text_count && (animate_ms || progress_ms))
	    add_text(text, text_count);

	get_ms_time_run();

	if (animate_ms) {
		bool never_stop = !stop_ms;
		long int time_left = stop_ms;
		int period = animate_ms / image_count;

		if (image_count < 2) {
			printf("Animating requires at least 2 images\n");
		}

		get_ms_time_run();

		i = 0;
		never_stop = !stop_ms;
		while (never_stop || time_left > 0) {	        
			if(loadLogo(images[i], images_dir))
				printf("\"%s\" not found in /res/images/\n", images[i]);
            else
			    showLogo();

			if (wait_signalfd(sigfd, period))
				break;
			time_left -= period;
			i++;
			i = i % image_count;
		}

	    get_ms_time_run();

		goto cleanup;
	} else
	if (progress_ms) {
        if (image_count > 1 && progress_ms)
            printf("Can only show one image with progressbar\n");

        if (image_count && loadLogo(images[0], images_dir))
            printf("Image \"%s\" not found in /res/images/\n", images[0]);

        if (progress_ms > (1ULL<<31)) {
            printf("Cannot use a progress_ms value bigger than 2^31\n");
            progress_ms = (1ULL<<31);
        }

        if(progress_ms < 100) {
            osUpdateScreenShowProgress(100);
	        wait_signalfd(sigfd, progress_ms);
            goto cleanup;
        }

        int wtme = progress_ms/100, trst = progress_ms, step = 1;

        get_ms_time_run();

        if(wtme < 10) {
            wtme = progress_ms/10;
            step = 10;
        }
        for (i = 0; i <= 100; i += step) {
            osUpdateScreenShowProgress(i);
            if (wait_signalfd(sigfd, wtme))
                break;
            trst -= wtme;
            if(trst < wtme)
                break;
        }
        if(i < 100)
            osUpdateScreenShowProgress(100);
        if(trst > 0)
            wait_signalfd(sigfd, trst);

        get_ms_time_run();

        printf("progress bar ended with wtme: %d, trst: %d, i: %d\n",
            wtme, trst, i);

        goto cleanup;
	} else
	if (image_count) {
	    get_ms_time_run();

		if(loadLogo(images[0], images_dir))
			printf("Image \"%s\" not found in /res/images/\n", images[0]);
        else
		    showLogo();
	}

	get_ms_time_run();

	/* In case there is text to add, add it to both sides of the "flip" */
	if(text_count)
	    add_text(text, text_count);

	get_ms_time_run();

	if (text_count || image_count) {
		wait_signalfd(sigfd, stop_ms);
		goto cleanup;
	} else {
        if (app_font_multipl) {
            printf("The font multiplier will be ingored without text");
        }
        if (app_text_xpos || app_text_ypos) {
            printf("The x-pos and y-pos will be ingored without text");
        }
    }

cleanup:
    if (!do_cleanup)
        goto out;
    if (sigfd != -1)
	    close(sigfd);
	sigfd = -1;
    osUpdateScreenExit();
out:
	return ret;
}
