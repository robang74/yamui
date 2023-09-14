/*
 * Simple screen saver daemon. Turns off the display after idle timeout.
 * Turns the display on on any event from /dev/input/event* files.
 * On exit turns the display on.
 *
 * Copyright (C) 2015 Jolla Ltd.
 * Contact: Igor Zhbanov <igor.zhbanov@jolla.com>
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

#include <errno.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/wait.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/select.h>

#include <linux/input.h>

/*#define DEBUG*/
#include "yamui-tools.h"
#include "minui/minui.h"

#define NBITS(x)		((((x) - 1) / __BITS_PER_LONG) + 1)
#define BIT(arr, bit)		((arr[(bit) / __BITS_PER_LONG] >> \
				 ((bit) % __BITS_PER_LONG)) & 1)

#define DISPLAY_CONTROL		"/sys/class/graphics/fb0/blank"
#define DISPLAY_CONTROL_DRM	"/sys/class/backlight/panel0-backlight/brightness"
#define MAX_DEVICES		    256
#define DISPLAY_OFF_TIME     30 /* seconds */

static const bool display_off_disabled = 1; //RAF: a user space script does

const char *app_name = "screensaverd";
sig_atomic_t volatile running = 1;

char *display_control = NULL;
int display_control_off_value = 1;
int display_control_on_value = 1024;

typedef enum {
	state_unknown = -1,
	state_off,
	state_on
} display_state_t;

static display_state_t display_state = state_unknown;

/* ------------------------------------------------------------------------ */

/* Check for input device type. Returns 0 if button or touchscreen. */
static int
check_device_type(int fd, const char *name)
{
	unsigned long bits[EV_MAX][NBITS(KEY_MAX)];

	memset(bits, '\0', sizeof(bits));
	if (ioctl(fd, EVIOCGBIT(0, EV_MAX), bits[0]) == -1) {
		errorf("ioctl(, EVIOCGBIT(0, ), ) error on event device %s",
		       name);
		return -1;
	}

	if (BIT(bits[0], EV_ABS)) {
		if (ioctl(fd, EVIOCGBIT(EV_ABS, KEY_MAX), bits[EV_ABS]) == -1)
			errorf("ioctl(, EVIOCGBIT(EV_ABS, ), ) error on event"
			       " device %s", name);
		else if (BIT(bits[EV_ABS], ABS_MT_POSITION_X) &&
			 BIT(bits[EV_ABS], ABS_MT_POSITION_Y)) {
			debugf("Device %s supports multi-touch events.", name);
			return 0;
		}
	}

	if (BIT(bits[0], EV_KEY)) {
		if (ioctl(fd, EVIOCGBIT(EV_KEY, KEY_MAX), bits[EV_KEY]) == -1)
			errorf("ioctl(, EVIOCGBIT(EV_KEY, ), ) error on event"
			       " device %s", name);
		else if (BIT(bits[EV_KEY], KEY_POWER)		||
			 BIT(bits[EV_KEY], KEY_VOLUMEDOWN)	||
			 BIT(bits[EV_KEY], KEY_VOLUMEUP)	||
			 BIT(bits[EV_KEY], KEY_OK)		||
			 BIT(bits[EV_KEY], KEY_ENTER)) {
			debugf("Device %s supports needed key events.", name);
			return 0;
		}
	}

	debugf("Skipping unsupported device %s.", name);
	return -1;
}

/* ------------------------------------------------------------------------ */

static int
sysfs_write_int(const char *fname, int val)
{
	FILE *f;

	if (!(f = fopen(fname, "w"))) {
		errorf("Can't open \"%s\" for writing", fname);
		return -1;
	}

	fprintf(f, "%d\n", val);
	fclose(f);
	return 0;
}

/* ------------------------------------------------------------------------ */

#define USE_READ_FOR_PWKEY_CMD 0

static int
turn_display_on(void)
{
	int ret;

	if (display_state == state_on)
		return 0;

	display_state = state_on;
	printf("Turning display on.\n");
	ret = sysfs_write_int(display_control, display_control_on_value);
#if 0
#ifdef __arm___
	gr_restore(); /* Qualcomm specific. TODO: implement generic solution. */
#endif /* __arm__ */
#endif

#if USE_READ_FOR_PWKEY_CMD
    static char cmdstr[1024], *fname = NULL;
    static int fd = -1, size;
    while(fd == -1) {
        fname = getenv("PWKEY_CMD_FILE");
        if(fname)
            fd = open(fname, O_RDONLY);
        if(fd == -1) {
            if(errno != ENOENT)
                fprintf(stderr,"ERROR: open(%s) failed, errno(%d): %s\n",
                    fname, errno, strerror(errno));
            break;
        }
        size = read(fd, cmdstr, 1024);
        if(!size) {
            fprintf(stderr,"WARNING: read(%s) returned zero size\n", fname);
            close(fd);
            fd = 0;
        } else
        if(size == 1024) {
            fprintf(stderr,"WARNING: read(%s) returned max size\n", fname);
            close(fd);
            fd = 0;
        } else
        if(size == -1) {
            fprintf(stderr,"ERROR: read(%s) failed, errno(%d): %s\n",
                fname, errno, strerror(errno));
            close(fd);
            fd = 0;
        } else
            close(fd);
        break;
    }
    if(fd > 0) system(cmdstr);
#else
    //RAF: this way is much simpler but the file should be executable. On the
    //     other side, the excutable flag could be pourposely switched to enable
    //     or disable the execution of the command by the yamui-screensaverd.
    static char *fname = NULL;
    if(!fname) fname = getenv("PWKEY_CMD_FILE");

    if(fname) {
        FILE *pf = popen(fname, "r");
        if(!pf) {
            fprintf(stderr,"ERROR: popen(%s) failed, errno(%d): %s\n",
                fname, errno, strerror(errno));
        } else {
            char str[16];
            if(fgets(str, sizeof(str), pf)) {
                for(int i = sizeof(str)-1; i >= 0; i--)
                    if(str[i] == '\n') str[i] = 0;
                printf("fgets(%s) on fileno(%d) returned: %s\n",
                    fname, fileno(pf), str);
            } else
                fprintf(stderr,"ERROR: fgets(%s) failed, errno(%d): %s\n",
                    fname, errno, strerror(errno));
            pclose(pf);
            int pid = atoi(str);
            if(pid > 0) {
                if(waitpid(pid, NULL, WNOHANG | WUNTRACED | WCONTINUED) == -1)
                    fprintf(stderr,"ERROR: waitpid(%d) failed, errno(%d): %s\n",
                        pid, errno, strerror(errno));
            } else
                fprintf(stderr,"ERROR: pid(%d) is not valid\n", pid);
        }
    }
#if 0 /* The system() does not return but popen() does */
    if(fname && system(fname)) {
        fprintf(stderr,"ERROR: read(%s) failed, errno(%d): %s\n",
            fname, errno, strerror(errno));
    } else
        printf("Command by system(%s) completed.\n", fname);
#endif
#endif /* USE_SYSTEM_FOR_PWKEY_CMD */

	return ret;
}

/* ------------------------------------------------------------------------ */

static int
turn_display_off(void)
{
	if (display_off_disabled || display_state == state_off)
		return 0;

	printf("Turning display off.\n");
	display_state = state_off;
#if 0
#ifdef __arm__
	gr_save(); /* Qualcomm specific. TODO: implement generic solution. */
#endif /* __arm__ */
#endif
	return sysfs_write_int(display_control, display_control_off_value);
}

/* ------------------------------------------------------------------------ */

static void
signal_handler(int sig UNUSED)
{
	running = 0;
}

/* ------------------------------------------------------------------------ */

int
main(void)
{
	int fds[MAX_DEVICES], num_fds = 0, ret = EXIT_SUCCESS;

	setlinebuf(stdout);
	setlinebuf(stderr);

	if (open_fds(fds, &num_fds, MAX_DEVICES, check_device_type) == -1)
		return EXIT_FAILURE;

	int have_fb0 = 0;
	/* the drm backend doesn't support multiple clients */
	have_fb0 = !access("/dev/fb0", F_OK) || !access("/dev/graphics/fb0", F_OK);
#if 0
	if (!have_fb0) {
#ifdef __arm__
		/* Qualcomm specific. TODO: implement generic solution. */
		if (gr_init(false)) {
			errorf("Failed gr_init().\n");
			close_fds(fds, num_fds);
			return EXIT_FAILURE;
		}
#endif /* __arm__ */
	}
#endif
	
	if (have_fb0) {
		printf("framebuffer found, using it.\n");
		display_control = DISPLAY_CONTROL;
	} else {
		printf("framebuffer not found, using drm.\n");
		display_control = DISPLAY_CONTROL_DRM;
	}

	if (getenv("DISPLAY_BRIGHTNESS_PATH") != NULL) {
		display_control = getenv("DISPLAY_BRIGHTNESS_PATH");
	}
	if (getenv("DISPLAY_BRIGHTNESS") != NULL) {
		display_control_on_value = atoi(getenv("DISPLAY_BRIGHTNESS"));
	}
	
	printf("path: %s\nmax brightness: %d\n", display_control,
		display_control_on_value);

	debugf("Started");
	signal(SIGINT,  signal_handler);
	signal(SIGTERM, signal_handler);

	while (running) { /* Main loop */
		int i, rv, max_fd = 0;
		fd_set rfds;
		struct timeval tv;

		FD_ZERO(&rfds);
		for (i = 0; i < num_fds; i++) {
			FD_SET(fds[i], &rfds);
			if (fds[i] > max_fd)
				max_fd = fds[i];
		}

		tv.tv_sec  = DISPLAY_OFF_TIME;
		tv.tv_usec = 0;

		printf("wait on select(%d) for an event\n", max_fd);
		rv = select(max_fd + 1, &rfds, NULL, NULL, &tv);
		if (rv > 0) {
			for (i = 0; i < num_fds; i++) {
				if (FD_ISSET(fds[i], &rfds)) {
					ret_t r;

					r = handle_events(fds[i], NULL);
					if (r == ret_continue) {
					    printf("skip event, fds[%d]: %d\n", i, fds[i]);
						continue;
					}
					printf("stop running, fds[%d]: %d\n", i, fds[i]);

					ret = get_exit_status(r);
					running = 0;
					break;
				}
			}
			turn_display_on();
		} else if (rv == 0) { /* Timeout */
			turn_display_off();
		} else { /* Error or signal */
		    fprintf(stderr, "ERROR: select(%d) failed, errno(%d): %s\n",
		        max_fd, errno, strerror(errno));
			if (errno != EINTR) {
				errorf("Error on select()");
				ret = EXIT_FAILURE;
			}
			break;
		}
	}

	turn_display_on();
#if 0
#ifdef __arm__
	if (have_fb0) {
		gr_exit(); /* Qualcomm specific. TODO: implement generic solution. */
	}
#endif /* __arm__ */
#endif
	close_fds(fds, num_fds);
	debugf("Terminated");
	return ret;
}
