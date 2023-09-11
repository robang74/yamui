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
static __attribute__((unused)) lld m_gettimems = -1;
static __attribute__((unused)) lld u_gettimems = -1;
static __attribute__((unused)) lld n_gettimems = -1;
#else
extern lld m_gettimems;
extern lld u_gettimems;
extern lld n_gettimems;
#endif

lld get_time_ms(lld tms, unsigned line, const char *file);
lld get_time_us(lld tus, unsigned line, const char *file);
lld get_time_ns(lld tns, unsigned line, const char *file);

#define get_ms_time_run() ({ lld _a = m_gettimems; m_gettimems = get_time_ms(_a, __LINE__, __FILE__); })
#define get_ms_time_now() ({ m_gettimems = get_time_ms(0, 0, NULL); })

#define get_us_time_run() ({ lld _a = u_gettimems; u_gettimems = get_time_us(_a, __LINE__, __FILE__); })
#define get_us_time_now() ({ u_gettimems = get_time_us(0, 0, NULL); })

#define get_ns_time_run() ({ lld _a = n_gettimems; n_gettimems = get_time_ns(_a, __LINE__, __FILE__); })
#define get_ns_time_now() ({ n_gettimems = get_time_ns(0, 0, NULL); })

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

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* _GET_TIME_MS_H_ */

#ifndef MSTIME_HEADER_ONLY

lld m_gettimems = -1;
lld u_gettimems = -1;
lld n_gettimems = -1;

lld get_time_ms(lld tms, unsigned line, const char *file)
{
    long ms;
    time_t s, sms;
    struct timespec spec;

    clock_gettime(CLOCK_REALTIME, &spec);
    s   = spec.tv_sec;
    ms  = MLN_DIV(spec.tv_nsec);
    sms = (MIL * s) + ms;
    
    //printf("debug> tms:%lld sms:%ld\n", tms, sms);

    if(1 || tms > 0) {
        char str[128]; str[0] = 0;
        if(line) {
            snprintf(str, 127, "=-> %s%s%03d: ",
                file ? file : "", file ? ":" : "", line);
            str[127] = 0;
        }
        if(tms > 0) {
            tms = sms - tms;
            printf("%s+%llu.%03llu\n", str, MIL_DIV(tms), MIL_RMN(tms));
        } else {
            printf("%s=%ld.%03ld\n", str, s, ms);
        }
    } else
    if(!tms)
        printf("%ld.%03ld\n", s, ms);

    return sms;
}

lld get_time_us(lld tus, unsigned line, const char *file)
{
    long us;
    time_t s, sus;
    struct timespec spec;

    clock_gettime(CLOCK_REALTIME, &spec);
    s   = spec.tv_sec;
    us  = MIL_DIV(spec.tv_nsec);
    sus = (MLN * s) + us;

    if(1 || tus > 0) {
        char str[128]; str[0] = 0;
        if(line) {
            snprintf(str, 127, "=-> %s%s%03d: ",
                file ? file : "", file ? ":" : "", line);
            str[127] = 0;
        }
        if(tus > 0) {
            tus = sus - tus;
            printf("%s+%llu.%06llu\n", str, MLN_DIV(tus), MLN_RMN(tus));
        } else {
            printf("%s=%ld.%06ld\n", str, s, us);
        }
    } else
    if(!tus)
        printf("%ld.%06ld\n", s, us);

    return sus;
}

lld get_time_ns(lld tns, unsigned line, const char *file)
{
    long ns;
    time_t s, sns;
    struct timespec spec;

    clock_gettime(CLOCK_REALTIME, &spec);
    s   = spec.tv_sec;
    ns  = spec.tv_nsec;
    sns = (MLD * s) + ns;

    if(1 || tns > 0) {
        char str[128]; str[0] = 0;
        if(line) {
            snprintf(str, 127, "=-> %s%s%03d: ",
                file ? file : "", file ? ":" : "", line);
            str[127] = 0;
        }
        if(tns > 0) {
            tns = sns - tns;
            printf("%s+%llu.%09llu\n", str, MLD_DIV(tns), MLD_RMN(tns));
        } else {
            printf("%s=%ld.%09ld\n", str, s, ns);
        }
    } else {
        if(!tns) printf("%ld.%09ld\n", s, ns);
        s = (MLD * s) + ns;
    }

    return sns;
}

#endif /* MSTIME_HEADER_ONLY */
