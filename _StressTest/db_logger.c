/* Copyright 2018 by Dexcom, Inc.  All rights reserved. */

#include "db_logger.h"
#include "dbg.h"
//#include "dxcm_hw_time.h"
#include "app_timer.h"
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>

#define MAX_RTC_COUNTER_VAL     0x00FFFFFF                                  /**< Maximum value of the RTC counter. */

/**@brief Function for returning the current value of the RTC1 counter.
 *
 * @return     Current value of the RTC1 counter.
 */
static __INLINE uint32_t rtc1_counter_get(void)
{
    return NRF_RTC1->COUNTER;
}


/**@brief Function for computing the difference between two RTC1 counter values.
 *
 * @return     Number of ticks elapsed from ticks_old to ticks_now.
 */
static __INLINE uint32_t ticks_diff_get(uint32_t ticks_now, uint32_t ticks_old)
{
    return ((ticks_now - ticks_old) & MAX_RTC_COUNTER_VAL);
}

void time_log(const char* fmt, ...)
{
  static unsigned _lasttick = 0;
  unsigned t = rtc1_counter_get();
  unsigned d = ticks_diff_get(t, _lasttick);
  _lasttick = t;
	t= t * 1000 / (MAX_RTC_COUNTER_VAL+1);
  dbgmsg("dt=%ums", d);
//  NRF_LOG_INFO(fmt);
	
}
