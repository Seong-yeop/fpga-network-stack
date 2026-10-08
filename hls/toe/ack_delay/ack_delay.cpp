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

#include "ack_delay.hpp"

using namespace hls;

void ack_scheduler(	stream<ackRequest>&		requestIn,
					stream<ackClear>&		clearIn,
					stream<ap_uint<16> >&	ackOut)
{
#pragma HLS PIPELINE II=1
#pragma HLS INLINE off

	static ackEntry ad_table[MAX_SESSIONS];
	#pragma HLS bind_storage variable=ad_table type=RAM_2P impl=BRAM
	#pragma HLS aggregate  variable=ad_table compact=bit
	#pragma HLS DEPENDENCE variable=ad_table inter false

	static ap_uint<16> ad_pointer = 0;
	static bool ad_fwValid[AD_FORWARD_DEPTH] = {};
	static ap_uint<16> ad_fwId[AD_FORWARD_DEPTH];
	static ackEntry ad_fwEntry[AD_FORWARD_DEPTH];
	static bool ad_fwZero[AD_FORWARD_DEPTH];
	static bool ad_fwOne[AD_FORWARD_DEPTH];
	#pragma HLS ARRAY_PARTITION variable=ad_fwValid complete
	#pragma HLS ARRAY_PARTITION variable=ad_fwId complete
	#pragma HLS ARRAY_PARTITION variable=ad_fwEntry complete
	#pragma HLS ARRAY_PARTITION variable=ad_fwZero complete
	#pragma HLS ARRAY_PARTITION variable=ad_fwOne complete

	ackRequest req;
	ackClear clr;
	ap_uint<2> op = 0;
	ap_uint<16> id = ad_pointer;
	if (!requestIn.empty())
	{
		requestIn.read(req);
		id = req.sessionID;
		op = 1;
	}
	else if (!clearIn.empty())
	{
		clearIn.read(clr);
		id = clr.sessionID;
		op = 2;
	}
	else
	{
		ad_pointer = (ad_pointer == MAX_SESSIONS-1) ? ap_uint<16>(0) : ap_uint<16>(ad_pointer+1);
	}

	ackEntry e = ad_table[id];
	bool tz = e.time == 0;
	bool t1 = e.time == 1;
	for (int k = AD_FORWARD_DEPTH-1; k >= 0; k--)
	{
	#pragma HLS UNROLL
		if (ad_fwValid[k] && ad_fwId[k] == id)
		{
			e = ad_fwEntry[k];
			tz = ad_fwZero[k];
			t1 = ad_fwOne[k];
		}
	}

	bool t2 = e.time == 2;
	bool pz = e.pend == 0;
	ap_uint<8> time = e.time;
	bool nz = tz;
	bool n1 = t1;
	ap_uint<2> keep = e.pend;
	ap_uint<2> less = e.pend - 1;
	bool want = !pz;
	switch (op)
	{
	case 1:
		if (req.nodelay)
		{
			time = 0;
			nz = true;
			n1 = false;
			keep = (e.pend == 3) ? ap_uint<2>(3) : ap_uint<2>(e.pend + 1);
			less = (e.pend == 3) ? ap_uint<2>(2) : e.pend;
			want = true;
		}
		else if (pz && tz)
		{
			time = AD_TIME;
			nz = AD_TIME == 0;
			n1 = AD_TIME == 1;
		}
		else if (pz)
		{
			time = 0;
			nz = true;
			n1 = false;
			keep = 1;
			less = 0;
			want = true;
		}
		break;
	case 2:
		time = 0;
		nz = true;
		n1 = false;
		want = false;
		if (clr.all)
		{
			keep = 0;
		}
		break;
	default:
		if (!tz && !t1)
		{
			time = e.time - 1;
			n1 = t2;
		}
		else if (t1)
		{
			time = 0;
			nz = true;
			n1 = false;
			if (pz)
			{
				keep = 1;
				less = 0;
				want = true;
			}
		}
		break;
	}

	bool sent = false;
	if (want)
	{
		sent = ackOut.write_nb(id);
	}
	e.time = time;
	e.pend = sent ? less : keep;

	ad_table[id] = e;
	for (int k = AD_FORWARD_DEPTH-1; k > 0; k--)
	{
	#pragma HLS UNROLL
		ad_fwValid[k] = ad_fwValid[k-1];
		ad_fwId[k] = ad_fwId[k-1];
		ad_fwEntry[k] = ad_fwEntry[k-1];
		ad_fwZero[k] = ad_fwZero[k-1];
		ad_fwOne[k] = ad_fwOne[k-1];
	}
	ad_fwValid[0] = true;
	ad_fwId[0] = id;
	ad_fwEntry[0] = e;
	ad_fwZero[0] = nz;
	ad_fwOne[0] = n1;
}

void ack_delay(	stream<extendedEvent>&	input,
				stream<ap_uint<16> >&	ackIn,
				stream<extendedEvent>&	output,
				stream<ackClear>&		clearOut,
				stream<ap_uint<1> >&	readCountFifo,
				stream<ap_uint<1> >&	writeCountFifo)
{
#pragma HLS PIPELINE II=1

	extendedEvent ev;
	ap_uint<16> id;

	if (!ackIn.empty())
	{
		ackIn.read(id);
		output.write(extendedEvent(event(ACK, id)));
		writeCountFifo.write(1);
	}
	else if (!input.empty())
	{
		input.read(ev);
		readCountFifo.write(1);
		output.write(ev);
		writeCountFifo.write(1);
		bool kill = ev.type == SYN || ev.type == SYN_ACK || (ev.type == RST && ev.rt_count != 0);
		bool carry = (ev.type == TX && ev.length != 0) || ev.type == FIN;
		if (kill || carry)
		{
			clearOut.write_nb(ackClear(ev.sessionID, kill));
		}
	}
}
