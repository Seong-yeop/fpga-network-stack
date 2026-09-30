#pragma once

#include "../toe_internals.hpp"

using namespace hls;

struct sackEntry
{
	ap_uint<3>	valid;
	ap_uint<32>	left0;
	ap_uint<32>	right0;
	ap_uint<32>	left1;
	ap_uint<32>	right1;
	ap_uint<32>	left2;
	ap_uint<32>	right2;
};

void sack_scoreboard(	stream<sackRequest>&		rxEng2sack_req,
						stream<ap_uint<16> >&		txEng2sack_clear,
						stream<sackReply>&			sack2rxEng_rsp);
