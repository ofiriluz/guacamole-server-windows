/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include <guacamole/config.h>

#include <guacamole/timestamp.h>
#ifdef HAVE_BOOST
#include <boost/thread.hpp>
#include <boost/date_time.hpp>
#elif defined HAVE_LIBPTHREAD
#include <sys/time.h>
#endif

#if defined(HAVE_CLOCK_GETTIME) || defined(HAVE_NANOSLEEP)
#include <time.h>
#endif
#ifdef HAVE_BOOST
#include <time.h>
#include <windows.h>

#if defined(_MSC_VER) || defined(_MSC_EXTENSIONS)
#define DELTA_EPOCH_IN_MICROSECS  11644473600000000Ui64
#else
#define DELTA_EPOCH_IN_MICROSECS  11644473600000000ULL
#endif

struct timezone
{
	int  tz_minuteswest; /* minutes W of Greenwich */
	int  tz_dsttime;     /* type of dst correction */
};

int gettimeofday(struct timeval *tv, struct timezone *tz)
{
	FILETIME ft;
	unsigned __int64 tmpres = 0;
	static int tzflag = 0;

	if (NULL != tv)
	{
		GetSystemTimeAsFileTime(&ft);

		tmpres |= ft.dwHighDateTime;
		tmpres <<= 32;
		tmpres |= ft.dwLowDateTime;

		tmpres /= 10;  /*convert into microseconds*/
					   /*converting file time to unix epoch*/
		tmpres -= DELTA_EPOCH_IN_MICROSECS;
		tv->tv_sec = (long)(tmpres / 1000000UL);
		tv->tv_usec = (long)(tmpres % 1000000UL);
}

	if (NULL != tz)
	{
		if (!tzflag)
		{
			_tzset();
			tzflag++;
		}
		tz->tz_minuteswest = _timezone / 60;
		tz->tz_dsttime = _daylight;
	}

	return 0;
}
#endif

guac_timestamp guac_timestamp_current() {

#ifdef HAVE_CLOCK_GETTIME

    struct timespec current;

    /* Get current time */
    clock_gettime(CLOCK_REALTIME, &current);
    
    /* Calculate milliseconds */
    return (guac_timestamp) current.tv_sec * 1000 + current.tv_nsec / 1000000;

#elif HAVE_LIBPTHREAD

    struct timeval current;

    /* Get current time */
    gettimeofday(&current, NULL);
    
    /* Calculate milliseconds */
    return (guac_timestamp) current.tv_sec * 1000 + current.tv_usec / 1000;
#elif defined HAVE_BOOST
	//LARGE_INTEGER fq, t;
	//QueryPerformanceFrequency(&fq);
	//QueryPerformanceCounter(&t);
	//return ((1000000 * t.QuadPart) / fq.QuadPart) / 1000; // MS
	//boost::posix_time::ptime time_epoch(boost::gregorian::date(1970, 1, 1));
	//boost::posix_time::ptime current = boost::posix_time::microsec_clock::local_time();
	//return (current - time_epoch).total_milliseconds();
	struct timeval current;

	/* Get current time */
	gettimeofday(&current, NULL);
	
	/* Calculate milliseconds */
	return (guac_timestamp)current.tv_sec * 1000 + current.tv_usec / 1000;
#endif

}

void guac_timestamp_msleep(int duration) {

#ifdef HAVE_BOOST
	boost::this_thread::sleep(boost::posix_time::milliseconds(duration));
#elif defined HAVE_LIBPTHREAD
    /* Split milliseconds into equivalent seconds + nanoseconds */
    struct timespec sleep_period = {
        .tv_sec  =  duration / 1000,
        .tv_nsec = (duration % 1000) * 1000000
    };

    /* Sleep for specified interval */
    nanosleep(&sleep_period, NULL);
#endif
}

