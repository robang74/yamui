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

#include <stdio.h>
#include <time.h>

static long long int __attribute__((unused)) m_gettimems = -1;
static long long int __attribute__((unused)) u_gettimems = -1;
static long long int __attribute__((unused)) n_gettimems = -1;

long long int get_time_ms(long long int tms, unsigned line);
long long int get_time_us(long long int tus, unsigned line);
long long int get_time_ns(long long int tns, unsigned line);

#define get_ms_time_run() { m_gettimems = get_time_ms(m_gettimems, __LINE__); }
#define get_ms_time_now() { m_gettimems = get_time_ms(0, 0); }

#define get_us_time_run() { u_gettimems = get_time_us(u_gettimems, __LINE__); }
#define get_us_time_now() { u_gettimems = get_time_us(0, 0); }

#define get_ns_time_run() { n_gettimems = get_time_ns(n_gettimems, __LINE__); }
#define get_ns_time_now() { n_gettimems = get_time_ns(0, 0); }

#define MIL (1000ULL)
#define MLN (1000000ULL)
#define MLD (1000000000ULL)

#define INT_DIV(a, b) ( (a + (b>>1)) / b )
#define INT_RMN(a, b) (a%b)

#define MIL_DIV(a) INT_DIV(a, MIL)
#define MLN_DIV(a) INT_DIV(a, MLN)
#define MLD_DIV(a) INT_DIV(a, MLD)

#define MIL_RMN(a) INT_DIV(a, MIL)
#define MLN_RMN(a) INT_DIV(a, MLN)
#define MLD_RMN(a) INT_DIV(a, MLD)

#ifndef INCLUDE_H_ONLY

long long int
get_time_ms(long long int tms, unsigned line)
{
    long ms;
    time_t s;
    struct timespec spec;

    clock_gettime(CLOCK_REALTIME, &spec);
    s  = spec.tv_sec;
    ms = MLN_DIV(spec.tv_nsec);

    if(tms > 0) {
        char str[32]; str[0] = 0;
        if(line) snprintf(str, 31, "=-> %03d: ", line);
        str[31] = 0; s = (MIL * s) + ms; tms = s-tms;
        printf("%s+%llu.%03llu\n", str, MIL_DIV(tms), MIL_RMN(tms));
    } else {
        if(!tms) printf("%ld.%03ld\n", s, ms);
        s = (MIL * s) + ms;
    }

    return s;
}

long long int
get_time_us(long long int tus, unsigned line)
{
    long us;
    time_t s;
    struct timespec spec;

    clock_gettime(CLOCK_REALTIME, &spec);
    s  = spec.tv_sec;
    us = MIL_DIV(spec.tv_nsec);

    if(tus > 0) {
        char str[32]; str[0] = 0;
        if(line) snprintf(str, 31, "-> %03d: ", line);
        str[31] = 0; s = (MLN * s) + us; tus = s-tus;
        printf("%s+%llu.%06llu\n", str, MLN_DIV(tus), MLN_RMN(tus));
    } else {
        if(!tus) printf("%ld.%06ld\n", s, us);
        s = (MLN * s) + us;
    }

    return s;
}

long long int
get_time_ns(long long int tns, unsigned line)
{
    long ns;
    time_t s;
    struct timespec spec;

    clock_gettime(CLOCK_REALTIME, &spec);
    s  = spec.tv_sec;
    ns = spec.tv_nsec;

    if(tns > 0) {
        char str[32]; str[0] = 0;
        if(line) snprintf(str, 31, "-> %03d: ", line);
        str[31] = 0; s = (MLD * s) + ns; tns = s-tns;
        printf("%s+%llu.%09llu\n", str, MLD_DIV(tns), MLD_RMN(tns));
    } else {
        if(!tns) printf("%ld.%09ld\n", s, ns);
        s = (MLD * s) + ns;
    }

    return s;
}


#endif
