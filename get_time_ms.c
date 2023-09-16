/*
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

#ifndef _GET_TIME_MS_H_
#define _GET_TIME_MS_H_

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <stdio.h>
#include <time.h>

typedef long long int lld;

#ifdef MSTIME_STATIC_VARS
static __attribute__((used)) lld m_gettimems = -1;
static __attribute__((used)) lld u_gettimems = -1;
static __attribute__((used)) lld n_gettimems = -1;
#else
extern lld m_gettimems;
extern lld u_gettimems;
extern lld n_gettimems;
#endif

lld _get_time_any(const lld t0, const char *file, const unsigned line,
    const char *const fmt, const char *const fmtb, const lld unit);

/*
lld get_time_ms(const lld t0, const char *file, const unsigned line);
lld get_time_us(const lld t0, const char *file, const unsigned line);
lld get_time_ns(const lld t0, const char *file, const unsigned line);
*/

#define MIL 1000
#define MLN 1000000
#define MLD 1000000000

#define INT_DIV(a, b) ({ __typeof__(a) _a = (a), _b = (b); (_a + (_b >> 1)) / _b; })
#define INT_RMN(a, b) ({ __typeof__(a) _a = (a), _b = (b); (_a % _b); })

#define MIL_DIV(a) INT_DIV(a, MIL)
#define MLN_DIV(a) INT_DIV(a, MLN)
#define MLD_DIV(a) INT_DIV(a, MLD)

#define MIL_RMN(a) INT_RMN(a, MIL)
#define MLN_RMN(a) INT_RMN(a, MLN)
#define MLD_RMN(a) INT_RMN(a, MLD)

#define MIL_FMT "%lld.%03lld\n"
#define MLN_FMT "%lld.%06lld\n"
#define MLD_FMT "%lld.%09lld\n"

#define get_time_ms(a, b, c) _get_time_any(a, b, c, "%s+" MIL_FMT, MIL_FMT, MIL)
#define get_time_us(a, b, c) _get_time_any(a, b, c, "%s+" MLN_FMT, MLN_FMT, MLN)
#define get_time_ns(a, b, c) _get_time_any(a, b, c, "%s+" MLD_FMT, MLD_FMT, MLD)

#ifdef MSTIME_TIME_ONLY
#define get_ms_time_run() ({ m_gettimems = get_time_ms(m_gettimems, 0, 0); })
#define get_us_time_run() ({ u_gettimems = get_time_us(u_gettimems, 0, 0); })
#define get_ns_time_run() ({ n_gettimems = get_time_ns(n_gettimems, 0, 0); })
#else
#define get_ms_time_run() ({ m_gettimems = get_time_ms(m_gettimems, __FILE__, __LINE__); })
#define get_us_time_run() ({ u_gettimems = get_time_us(u_gettimems, __FILE__, __LINE__); })
#define get_ns_time_run() ({ n_gettimems = get_time_ns(n_gettimems, __FILE__, __LINE__); })
#endif

#define get_ms_time_lbl(lbl) ({ m_gettimems = get_time_ms(m_gettimems, lbl, 0); })
#define get_us_time_lbl(lbl) ({ u_gettimems = get_time_us(u_gettimems, lbl, 0); })
#define get_ns_time_lbl(lbl) ({ n_gettimems = get_time_ns(n_gettimems, lbl, 0); })

#define get_ms_time_now() ({ m_gettimems = get_time_ms(0, NULL, 0); })
#define get_us_time_now() ({ u_gettimems = get_time_us(0, NULL, 0); })
#define get_ns_time_now() ({ n_gettimems = get_time_ns(0, NULL, 0); })

#define get_ms_time_rst() ({ m_gettimems = -1; get_ms_time_run(); })
#define get_us_time_rst() ({ u_gettimems = -1; get_us_time_run(); })
#define get_ns_time_rst() ({ n_gettimems = -1; get_ns_time_run(); })

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* _GET_TIME_MS_H_ */

#ifndef MSTIME_HEADER_ONLY

#define BUFSIZE 128

lld m_gettimems = -1;
lld u_gettimems = -1;
lld n_gettimems = -1;

lld _get_time_any(const lld t0, const char *file, const unsigned line,
    const char *const fmta, const char *const fmtb, const lld unit)
{
    lld s, rms, ctm;
    struct timespec spec;

    clock_gettime(CLOCK_REALTIME, &spec);
    s   = spec.tv_sec;
    rms = INT_DIV(spec.tv_nsec, MLD/unit);
    ctm = (unit * s) + rms;

    if(t0 > 0) {
        char str[BUFSIZE]; str[0] = 0;
        if(line) {
            snprintf(str, BUFSIZE, "=-> %s%s%03d: ",
                file ? file : "", file ? ":" : "", line);
        } else
        if(file) {
            snprintf(str, BUFSIZE, "=-> %s: ", file);
        }
        lld tdf = ctm - t0;
        str[BUFSIZE-1] = 0;
        printf(fmta, str, INT_DIV(tdf, unit), INT_RMN(tdf, unit));
    } else
    if(!t0)
        printf(fmtb, s, rms);

    return ctm;
}
/*
lld get_time_ms(const lld t0, const char *file, const unsigned line)
{
    lld s, rms, tdf;
    struct timespec spec;

    clock_gettime(CLOCK_REALTIME, &spec);
    s   = spec.tv_sec;
    rms  = MLN_DIV(spec.tv_nsec);
    tdf = (MIL * s) + rms;

    if(t0 > 0) {
        char str[BUFSIZE]; str[0] = 0;
        if(line) {
            snprintf(str, BUFSIZE, "=-> %s%s%04d: ",
                file ? file : "", file ? ":" : "", line);
        } else
        if(file) {
            snprintf(str, BUFSIZE, "=-> %s: ", file);
        }
        tdf -= t0;
        str[BUFSIZE-1] = 0;
        printf("%s+%lld.%03lld\n", str, MIL_DIV(tdf), MIL_RMN(tdf));
    } else
    if(!t0)
        printf("%lld.%03lld\n", s, rms);

    return tdf;
}

lld get_time_us(const lld t0, const char *file, const unsigned line)
{
    lld s, rms, tdf;
    struct timespec spec;

    clock_gettime(CLOCK_REALTIME, &spec);
    s   = spec.tv_sec;
    rms  = MIL_DIV(spec.tv_nsec);
    tdf = (MLN * s) + rms;

    if(t0 > 0) {
        char str[BUFSIZE]; str[0] = 0;
        if(line) {
            snprintf(str, BUFSIZE, "=-> %s%s%04d: ",
                file ? file : "", file ? ":" : "", line);
            str[BUFSIZE-1] = 0;
        } else
        if(file) {
            snprintf(str, BUFSIZE, "=-> %s: ", file);
        }
        tdf -= t0;
        str[BUFSIZE-1] = 0;
        printf("%s+%lld.%06lld\n", str, MLN_DIV(tdf), MLN_RMN(tdf));
    } else
    if(!t0)
        printf("%lld.%06lld\n", s, rms);

    return tdf;
}

lld get_time_ns(const lld t0, const char *file, const unsigned line)
{
    lld s, rms, tdf;
    struct timespec spec;

    clock_gettime(CLOCK_REALTIME, &spec);
    s   = spec.tv_sec;
    rms  = spec.tv_nsec;
    tdf = (MLD * s) + rms;

    if(t0 > 0) {
        char str[BUFSIZE]; str[0] = 0;
        if(line) {
            snprintf(str, BUFSIZE, "=-> %s%s%04d: ",
                file ? file : "", file ? ":" : "", line);
            str[BUFSIZE-1] = 0;
        } else
        if(file) {
            snprintf(str, BUFSIZE, "=-> %s: ", file);
        }
        tdf -= t0;
        str[BUFSIZE-1] = 0;
        printf("%s+%lld.%09lld\n", str, MLD_DIV(tdf), MLD_RMN(tdf));
    } else
    if(!t0)
        printf("%lld.%09lld\n", s, rms);

    return tdf;
}
*/
#endif /* MSTIME_HEADER_ONLY */
