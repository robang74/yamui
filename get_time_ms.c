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

long long unsigned
_get_time_ms(long long int tms, int line)
{
    long ms;
    time_t s;
    struct timespec spec;

    clock_gettime(CLOCK_REALTIME, &spec);
    s  = spec.tv_sec;
    ms = (spec.tv_nsec / 1.0e6);
    if(tms > 0) {
        printf("-> %03d: +%llu.%03llu\n", line, s-(tms/1000), ms-(tms%1000));
    } else if(!tms) {
        printf("%ld.%03ld\n", s, ms);
    }

    return s * 1000 + ms;
}
