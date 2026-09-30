/************************************************
Copyright (c) 2016, Xilinx, Inc.
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors
may be used to endorse or promote products derived from this software
without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.// Copyright (c) 2015 Xilinx, Inc.
************************************************/

#include "retransmit_timer.hpp"

using namespace hls;

void retransmit_timer(	stream<rxRetransmitTimerUpdate>&	rxEng2timer_clearRetransmitTimer,
						stream<txRetransmitTimerSet>&		txEng2timer_setRetransmitTimer,
						stream<event>&						rtTimer2eventEng_setEvent,
						stream<ap_uint<16> >&				rtTimer2stateTable_releaseState,
						stream<appNotification>&			rtTimer2rxApp_notification,
						stream<openStatus>&					rtTimer2txApp_notification)
{
#pragma HLS PIPELINE II=1

#pragma HLS aggregate  variable=rxEng2timer_clearRetransmitTimer compact=bit
#pragma HLS aggregate  variable=txEng2timer_setRetransmitTimer compact=bit
#pragma HLS aggregate  variable=rtTimer2eventEng_setEvent compact=bit
#pragma HLS aggregate  variable=rtTimer2stateTable_releaseState compact=bit
#pragma HLS aggregate  variable=rtTimer2rxApp_notification compact=bit

	static retransmitTimerEntry retransmitTimerTable[MAX_SESSIONS];
	#pragma HLS bind_storage variable=retransmitTimerTable type=RAM_T2P impl=BRAM
	#pragma HLS aggregate  variable=retransmitTimerTable compact=bit
	#pragma HLS DEPENDENCE variable=retransmitTimerTable inter false

	static ap_uint<16> rt_position = 0;
	static bool rt_fwValid[RT_FORWARD_DEPTH] = {};
	static ap_uint<16> rt_fwId[RT_FORWARD_DEPTH];
	static retransmitTimerEntry rt_fwEntry[RT_FORWARD_DEPTH];
	#pragma HLS ARRAY_PARTITION variable=rt_fwValid complete
	#pragma HLS ARRAY_PARTITION variable=rt_fwId complete
	#pragma HLS ARRAY_PARTITION variable=rt_fwEntry complete

	rxRetransmitTimerUpdate update;
	txRetransmitTimerSet set;
	ap_uint<2> operationSwitch = 0;
	ap_uint<16> currID = rt_position;
	if (!rxEng2timer_clearRetransmitTimer.empty())
	{
		rxEng2timer_clearRetransmitTimer.read(update);
		currID = update.sessionID;
		operationSwitch = 2;
	}
	else if (!txEng2timer_setRetransmitTimer.empty())
	{
		txEng2timer_setRetransmitTimer.read(set);
		currID = set.sessionID;
		operationSwitch = 1;
	}
	else
	{
		rt_position = (rt_position == MAX_SESSIONS-1) ? ap_uint<16>(0) : ap_uint<16>(rt_position+1);
	}

	retransmitTimerEntry currEntry = retransmitTimerTable[currID];
	for (int k = RT_FORWARD_DEPTH-1; k >= 0; k--)
	{
	#pragma HLS UNROLL
		if (rt_fwValid[k] && rt_fwId[k] == currID)
		{
			currEntry = rt_fwEntry[k];
		}
	}

	switch (operationSwitch)
	{
	case 2:
		currEntry.retries = 0;
		if (update.stop)
		{
			currEntry.active = false;
			currEntry.time = 0;
			currEntry.backoff = 0;
		}
		else
		{
			if (update.collapse)
			{
				currEntry.backoff = 0;
			}
			currEntry.active = true;
			currEntry.time = rtoInterval(currEntry.backoff);
		}
		break;
	case 1:
		currEntry.type = set.type;
		if (!currEntry.active)
		{
			currEntry.time = rtoInterval(currEntry.backoff);
			currEntry.active = true;
		}
		break;
	default:
		if (currEntry.active)
		{
			if (currEntry.time > 0)
			{
				currEntry.time--;
			}
			else if (!rtTimer2eventEng_setEvent.full())
			{
				currEntry.active = false;
				if (currEntry.retries < RT_MAX_RETRIES)
				{
					currEntry.retries++;
					if (currEntry.backoff < RT_MAX_BACKOFF)
					{
						currEntry.backoff++;
					}
					rtTimer2eventEng_setEvent.write(event(currEntry.type, currID, currEntry.retries));
				}
				else
				{
					currEntry.retries = 0;
					currEntry.backoff = 0;
					rtTimer2stateTable_releaseState.write(currID);
					if (currEntry.type == SYN)
					{
						rtTimer2txApp_notification.write(openStatus(currID, false));
					}
					else
					{
						rtTimer2rxApp_notification.write(appNotification(currID, true));
					}
				}
			}
		}
		break;
	}
	retransmitTimerTable[currID] = currEntry;
	for (int k = RT_FORWARD_DEPTH-1; k > 0; k--)
	{
	#pragma HLS UNROLL
		rt_fwValid[k] = rt_fwValid[k-1];
		rt_fwId[k] = rt_fwId[k-1];
		rt_fwEntry[k] = rt_fwEntry[k-1];
	}
	rt_fwValid[0] = true;
	rt_fwId[0] = currID;
	rt_fwEntry[0] = currEntry;
}
