/*
 *  Copyright (c) 2016, Nest Labs, Inc.
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. Neither the name of the copyright holder nor the
 *     names of its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file
 *   This file implements the OpenThread platform abstraction for the alarm.
 *
 * Modified by Qiang Gu(Qiang.Gu@synopsys.com) for embARC
 * \version 2016.12
 * \date 2016-11-7
 *
 */

#include <platform/alarm.h>
#include "platform-emsk.h"

#ifdef DEBUG
#undef DEBUG
#endif
#include "embARC_debug.h"

static uint32_t sCounter = 0;
static uint32_t expires;
static bool sIsRunning = false;

void emskAlarmInit(void)
{
	sCounter = OSP_GET_CUR_MS();
}

uint32_t otPlatAlarmGetNow(void)
{
	return (OSP_GET_CUR_MS() - sCounter);
}

void otPlatAlarmStartAt(otInstance *aInstance, uint32_t t0, uint32_t dt)
{
	(void)aInstance;
	expires = t0 + dt;
	sIsRunning = true;
}

void otPlatAlarmStop(otInstance *aInstance)
{
	(void)aInstance;
	sIsRunning = false;
}

void emskAlarmUpdateTimeout(int32_t *aTimeout)
{
	int32_t remaining;

	if (aTimeout == NULL)
	{
		return;
	}

	if (sIsRunning)
	{
		remaining = (int32_t)(expires - otPlatAlarmGetNow());

		if (remaining > 0)
		{
			*aTimeout = remaining;
		}
		else
		{
			*aTimeout = 0;
		}
	}
	else
	{
		*aTimeout = 10000;
	}
}

void emskAlarmProcess(otInstance *aInstance)
{
	int32_t remaining;

	if (sIsRunning)
	{
		remaining = (int32_t)(expires - otPlatAlarmGetNow());
		/* Testing */
		DBG("remaining time (AlarmProcess) = %d\r\n", remaining);

		if (remaining <= 0)
		{
			sIsRunning = false;
			otPlatAlarmFired(aInstance);
			DBG("Enable otPlatAlarmFired()\r\n");
		}

	}

}
