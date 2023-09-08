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

#define IMAGES_MAX	30

static struct option options[] = {
	{"animate",     required_argument, 0, 'a'},
	{"imagesdir",   required_argument, 0, 'i'},
	{"progressbar", required_argument, 0, 'p'},
	{"stopafter",   required_argument, 0, 's'},
	{"text",        required_argument, 0, 't'},
	{"fontmultipl", required_argument, 0, 'm'},
	{"help",        no_argument,       0, 'h'},
	{0, 0, 0, 0},
};

/* ------------------------------------------------------------------------ */

static int
wait_signalfd(int sigfd, unsigned long long int msecs)
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

/* ------------------------------------------------------------------------ */

static void
short_help(void)
{
	printf("USAGE: yamui [OPTIONS] [IMAGE(s)]\n");
}

/* ------------------------------------------------------------------------ */

static void
print_help(void)
{
	printf("  yamui - tool to display progress bar, logo, or small animation on UI\n");
	printf("  Usage:\n");
	short_help();
	printf("    IMAGE(s)   - png picture file names in DIR without .png extension\n");
	printf("                 NOTE: currently maximum of %d pictures supported\n",
	       IMAGES_MAX);
	printf("\n  OPTIONS:\n");
	printf("  --animate=PERIOD, -a PERIOD\n");
	printf("         Show IMAGEs (at least 2) in rotation over PERIOD ms\n");
	printf("  --imagesdir=DIR, -i DIR\n");
	printf("         Load IMAGE(s) from DIR, /res/images by default\n");
	printf("  --progressbar=TIME, -p TIME\n");
	printf("         Show a progess bar over TIME milliseconds\n");
	printf("  --stopafter=TIME, -s TIME\n");
	printf("         Stop showing the IMAGE(s) after TIME milliseconds\n");
	printf("  --text=STRING, -t STRING\n");
	printf("         Show STRING on the screen\n");
	printf("  --fontmultipl=FACTOR, -m FACTOR\n");
	printf("         Increase the font size by a factor between 1 and 16\n");
	printf("  --help, -h\n");
	printf("         Print this help\n");
}

/* ------------------------------------------------------------------------ */

/* Add text to both sides of the "flip" */
static void
add_text(char *text)
{
	int i = 0;
	if (!text)
		return;

	for (i = 0; i < 2; i++) {
		gr_color(255, 255, 255, 255);
		gr_text(20,20, text, 1);
		gr_flip();
	}
}

/* ------------------------------------------------------------------------ */

int
main(int argc, char *argv[])
{
	int c, option_index;
	unsigned long int animate_ms = 0;
	unsigned long long int stop_ms = 0;
	unsigned long long int progress_ms = 0;
	unsigned long long int app_font_multipl = 0;
	char * text = NULL;
	char * images[IMAGES_MAX];
	char * images_dir = "/res/images";
	int image_count = 0;
	int ret = 0;
	int i = 0;
	int sigfd = -1;
	sigset_t mask;

	setlinebuf(stdout);

	while (1) {
		c = getopt_long(argc, argv, "a:i:p:s:t:m:h", options,
				&option_index);
		if (c == -1)
			break;

		switch (c) {
		case 'a':
			printf("got animate %s ms\n", optarg);
			animate_ms = strtoul(optarg, (char **)NULL, 10);
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
			printf("got text \"%s\" to display\n", optarg);
            if (!app_font_multipl)
                app_font_multipl = 1;
            else
            if (app_font_multipl > 16) {
                printf("The font multiplier is out of range");
                print_help();
                exit(EXIT_FAILURE);
            }
			text = optarg;
			break;
        case 'm':
            printf("got font %s multipier\n", optarg);
            app_font_multipl = strtoull(optarg, NULL, 10);
            break;
		case 'h':
			print_help();
			goto out;
			break;
		default:
			printf("getopt returned character code 0%o\n", c);
			short_help();
			goto out;
			break;
		}
	}

	while (optind < argc && image_count < IMAGES_MAX)
		images[image_count++] = argv[optind++];

	if (osUpdateScreenInit())
		return -1;

	/* Allow SIGTERM and SIGINT to interrupt pselect() and move to cleanup */
	sigemptyset(&mask);
	sigaddset(&mask, SIGTERM);
	sigaddset(&mask, SIGINT);
	sigfd = signalfd(-1, &mask, 0);
	if (sigfd == -1) {
		printf("Could not create signal fd\n");
		goto cleanup;
	}
	if (sigprocmask(SIG_BLOCK, &mask, NULL) == -1) {
		printf("Could not block signals\n");
		goto cleanup;
	}

	/* In case there is text to add, add it to both sides of the "flip" */
	add_text(text);

	if (image_count == 1 && !progress_ms) {
		ret = loadLogo(images[0], images_dir);
		if (ret) {
			printf("Image \"%s\" not found in /res/images/\n",
			       images[0]);
			goto cleanup;
		}

		showLogo();
		wait_signalfd(sigfd, stop_ms);

		goto cleanup;
	}

	if (image_count <= 1 && progress_ms) {
		if (image_count == 1)
			loadLogo(images[0], images_dir);
		i = 0;
		while (i <= 100) {
			osUpdateScreenShowProgress(i);
			if (wait_signalfd(sigfd, progress_ms / 100))
				break;
			i++;
		}

		goto cleanup;
	}

	if (image_count > 1 && progress_ms) {
		printf("Can only show one image with progressbar\n");
		goto cleanup;
	}

	if (animate_ms) {
		bool never_stop;
		long int time_left = stop_ms;
		int period = animate_ms / image_count;

		if (image_count < 2) {
			printf("Animating requires at least 2 images\n");
			goto cleanup;
		}

		never_stop = !stop_ms;

		i = 0;
		while (never_stop || time_left > 0) {
			ret = loadLogo(images[i], images_dir);
			if (ret) {
				printf("\"%s\" not found in /res/images/\n",
				       images[i]);
				goto cleanup;
			}

			showLogo();
			if (wait_signalfd(sigfd, period))
				break;
			time_left -= period;
			i++;
			i = i % image_count;
		}

		goto cleanup;
	}

	if (text) {
		wait_signalfd(sigfd, stop_ms);
		goto cleanup;
	} else
    if (app_font_multipl) {
        printf("The font multiplier will be ingored without text");
    }

cleanup:
	if (sigfd != -1)
		close(sigfd);
	osUpdateScreenExit();
out:
	return ret;
}
