/*
 * Copyright (C) 2008 Philippe Gerum <rpm@xenomai.org>.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.

 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA.
 */

#ifndef _COPPERPLATE_CLOCKOBJ_H
#define _COPPERPLATE_CLOCKOBJ_H

#include <pthread.h>
#include <xeno_config.h>
#include <boilerplate/time.h>
#include <boilerplate/list.h>
#include <boilerplate/lock.h>
#include <copperplate/debug.h>

/*
 * We define the Copperplate clock as a monotonic, non-adjustable
 * one. This means that delays and timeouts won't be affected when the
 * kernel host date is changed. The implementation provides support
 * for absolute dates internally, with a per-clock epoch value, so
 * that different emulators can have different system dates.
 */
#ifdef CONFIG_XENO_RAW_CLOCK_ENABLED
#define CLOCK_COPPERPLATE  CLOCK_MONOTONIC_RAW
#else
#define CLOCK_COPPERPLATE  CLOCK_MONOTONIC
#endif

struct clockobj {
	pthread_mutex_t lock;
	struct timespec epoch;
	struct timespec offset;
#ifndef CONFIG_XENO_LORES_CLOCK_DISABLED
	unsigned int resolution;
	unsigned int frequency;
#endif
	const char *name;	/* __ref FIXME */
};

#ifdef __cplusplus
extern "C" {
#endif

void clockobj_set_date(struct clockobj *clkobj, ticks_t ticks);

void clockobj_get_date(struct clockobj *clkobj, ticks_t *pticks);

void clockobj_get_time(struct clockobj *clkobj,
		       ticks_t *pticks, ticks_t *ptsc);

ticks_t clockobj_get_tsc(void);

void clockobj_caltime_to_timeout(struct clockobj *clkobj, const struct tm *tm,
				 unsigned long rticks, struct timespec *ts);

void clockobj_caltime_to_ticks(struct clockobj *clkobj, const struct tm *tm,
			       unsigned long rticks, ticks_t *pticks);

void clockobj_ticks_to_caltime(struct clockobj *clkobj,
			       ticks_t ticks,
			       struct tm *tm,
			       unsigned long *rticks);

void clockobj_convert_clocks(struct clockobj *clkobj,
			     const struct timespec *in,
			     clockid_t clk_id,
			     struct timespec *out);

int clockobj_set_resolution(struct clockobj *clkobj,
			    unsigned int resolution_ns);

int clockobj_init(struct clockobj *clkobj,
		  const char *name, unsigned int resolution_ns);

int clockobj_destroy(struct clockobj *clkobj);

#ifdef __cplusplus
}
#endif

#ifdef CONFIG_XENO_COBALT

#include <cobalt/ticks.h>

static inline sticks_t clockobj_ns_to_tsc(sticks_t ns)
{
	return cobalt_ns_to_ticks(ns);
}

static inline sticks_t clockobj_tsc_to_ns(sticks_t tsc)
{
	return cobalt_ticks_to_ns(tsc);
}

static inline
void clockobj_ns_to_timespec(ticks_t ns, struct timespec *ts)
{
	unsigned long rem;

	ts->tv_sec = cobalt_divrem_billion(ns, &rem);
	ts->tv_nsec = rem;
}

#else /* CONFIG_XENO_MERCURY */

static inline sticks_t clockobj_ns_to_tsc(sticks_t ns)
{
	return ns;
}

static inline sticks_t clockobj_tsc_to_ns(sticks_t tsc)
{
	return tsc;
}

static inline
void clockobj_ns_to_timespec(ticks_t ns, struct timespec *ts)
{
	ts->tv_sec = ns / 1000000000ULL;
	ts->tv_nsec = ns - (ts->tv_sec * 1000000000ULL);
}

#endif /* CONFIG_XENO_MERCURY */

#ifdef CONFIG_XENO_LORES_CLOCK_DISABLED

static inline
void __clockobj_ticks_to_timeout(struct clockobj *clkobj,
				 clockid_t clk_id,
				 ticks_t ticks, struct timespec *ts)
{
	struct timespec now, delta;

	__RT(clock_gettime(clk_id, &now));
	clockobj_ns_to_timespec(ticks, &delta);
	timespec_add(ts, &now, &delta);
}

static inline
void __clockobj_ticks_to_timespec(struct clockobj *clkobj,
				  ticks_t ticks, struct timespec *ts)
{
	clockobj_ns_to_timespec(ticks, ts);
}

static inline
void clockobj_ticks_to_timespec(struct clockobj *clkobj,
				ticks_t ticks, struct timespec *ts)
{
	__clockobj_ticks_to_timespec(clkobj, ticks, ts);
}

static inline
unsigned int clockobj_get_resolution(struct clockobj *clkobj)
{
	return 1;
}

static inline
unsigned int clockobj_get_frequency(struct clockobj *clkobj)
{
	return 1000000000;
}

static inline sticks_t clockobj_ns_to_ticks(struct clockobj *clkobj,
					    sticks_t ns)
{
	return ns;
}

static inline sticks_t clockobj_ticks_to_ns(struct clockobj *clkobj,
					    sticks_t ticks)
{
	return ticks;
}

#else /* !CONFIG_XENO_LORES_CLOCK_DISABLED */

void __clockobj_ticks_to_timeout(struct clockobj *clkobj, clockid_t clk_id,
				 ticks_t ticks, struct timespec *ts);

void __clockobj_ticks_to_timespec(struct clockobj *clkobj,
				  ticks_t ticks, struct timespec *ts);

static inline
void clockobj_ticks_to_timespec(struct clockobj *clkobj,
				ticks_t ticks, struct timespec *ts)
{
	read_lock_nocancel(&clkobj->lock);
	__clockobj_ticks_to_timespec(clkobj, ticks, ts);
	read_unlock(&clkobj->lock);
}

static inline
unsigned int clockobj_get_resolution(struct clockobj *clkobj)
{
	return clkobj->resolution;
}

static inline
unsigned int clockobj_get_frequency(struct clockobj *clkobj)
{
	return clkobj->frequency;
}

sticks_t clockobj_ns_to_ticks(struct clockobj *clkobj,
			      sticks_t ns);

static inline sticks_t clockobj_ticks_to_ns(struct clockobj *clkobj,
					    sticks_t ticks)
{
	return ticks * clkobj->resolution;
}

#endif /* !CONFIG_XENO_LORES_CLOCK_DISABLED */

static inline
void clockobj_ticks_to_timeout(struct clockobj *clkobj,
			       ticks_t ticks, struct timespec *ts)
{
	__clockobj_ticks_to_timeout(clkobj, CLOCK_COPPERPLATE, ticks, ts);
}

#endif /* _COPPERPLATE_CLOCKOBJ_H */
