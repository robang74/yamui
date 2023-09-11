#include <unistd.h>
#include <stdlib.h>

#define MSTIME_TIME_ONLY
#define MSTIME_HEADER_ONLY
#include "get_time_ms.c"

static inline int
get_my_basename_index(const char *path)
{
    static int i, n = -1;
    if (n >= 0)
        return n;
    for(i = 0; path[i]; i++)
        if(path[i] == '/')
            n = i;
    return ++n;
}

static inline long long int
get_last_time_env(const char *varname)
{
    char *str = getenv(varname);
    if (str != NULL) {
        int i, n = -1;
        for(i = 0; str[i]; i++) {
            if(str[i] == '.')
                n = i;
            if(n > 0)
                str[i] = str[i+1];
        }
        return strtoul(str, (char **)NULL, 10);
    }
    return -1;
}

#define basename (&argv[0][get_my_basename_index(argv[0])])

int main (int argc, char **argv)
{
    int ret = 0;

    if(argc < 2) {
        if(basename[0] == 'm') {
            get_ms_time_now();
        } else
        if(basename[0] == 'u') {
            get_us_time_now();
        } else
        if(basename[0] == 'n') {
            get_ns_time_now();
        } else {
            //printf("%s(%d)='%c'\n", argv[0], n, argv[0][n]);
            ret = 1;
        }
    } else
    if(argv[1][0] == 'm') {
        get_ms_time_run();
        get_ms_time_run();
        usleep(1000000);
        get_ms_time_run();
    } else
    if(argv[1][0] == 'u') {
        get_us_time_run();
        get_us_time_run();
        usleep(1000000);
        get_us_time_run();
    } else
    if(argv[1][0] == 'n') {
        get_ns_time_run();
        get_ns_time_run();
        usleep(1000000);
        get_ns_time_run();
    } else
    if(argv[1][0] == '-') {
        long long int last = 0;
        if(basename[0] == 'm') {
            last = get_last_time_env("LAST_MS_TIME");
            if(last > 0)
                m_gettimems = last;
            get_ms_time_run();
        } else
        if(basename[0] == 'u') {
            last = get_last_time_env("LAST_US_TIME");
            if(last > 0)
                u_gettimems = last;
            get_us_time_run();
        } else
        if(basename[0] == 'n') {
            last = get_last_time_env("LAST_NS_TIME");
            if(last > 0)
                n_gettimems = last;
            get_ns_time_run();
        } else {
            //int n = get_my_basename_index(argv[0]);
            //printf("%s(%d)='%c'\n", argv[0],n, argv[0][n]);
            ret = 1;
        }
    } else
        ret = 1;

    if(ret) {
        printf("\nUSAGE: %s [h|m|u|n|-]\n", basename);
        printf("\nexamples:\n");
        printf("    mstime h # for this usage help\n");
        printf("    mstime u # for having us resolution, n for nanoseconds\n");
        printf("    export LAST_US_TIME=$(ustime); sleep 1; ustime -\n\n");
    }

    return ret;
}
