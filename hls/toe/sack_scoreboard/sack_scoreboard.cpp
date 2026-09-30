#include "sack_scoreboard.hpp"

using namespace hls;

static bool seqBefore(ap_uint<32> a, ap_uint<32> b)
{
#pragma HLS INLINE
	return ap_int<32>(a - b) < 0;
}

static bool seqAfter(ap_uint<32> a, ap_uint<32> b)
{
#pragma HLS INLINE
	return ap_int<32>(a - b) > 0;
}

static void trimBlock(ap_uint<1>& valid, ap_uint<32>& left, ap_uint<32>& right, ap_uint<32> ack)
{
#pragma HLS INLINE
	if (valid)
	{
		if (!seqAfter(right, ack))
		{
			valid = 0;
		}
		else if (seqBefore(left, ack))
		{
			left = ack;
		}
	}
}

static bool touches(ap_uint<32> l, ap_uint<32> r, ap_uint<1> valid, ap_uint<32> left, ap_uint<32> right)
{
#pragma HLS INLINE
	return valid && !seqAfter(l, right) && !seqBefore(r, left);
}

static sackEntry clearedEntry()
{
#pragma HLS INLINE
	sackEntry e;
	e.valid = 0;
	e.left0 = 0; e.right0 = 0; e.left1 = 0; e.right1 = 0; e.left2 = 0; e.right2 = 0;
	return e;
}

void sack_scoreboard(	stream<sackRequest>&		rxEng2sack_req,
						stream<ap_uint<16> >&		txEng2sack_clear,
						stream<sackReply>&			sack2rxEng_rsp)
{
#pragma HLS PIPELINE II=1

#pragma HLS aggregate  variable=rxEng2sack_req compact=bit
#pragma HLS aggregate  variable=sack2rxEng_rsp compact=bit

	static sackEntry sack_table[MAX_SESSIONS];
	#pragma HLS bind_storage variable=sack_table type=RAM_T2P impl=BRAM
	#pragma HLS aggregate  variable=sack_table compact=bit
	#pragma HLS DEPENDENCE variable=sack_table inter false

	enum fsmStateType {IDLE, MERGE_CMP, MERGE_APPLY, COALESCE, WRITE};
	static fsmStateType state = IDLE;
	static sackRequest req;
	static ap_uint<1> valid0 = 0, valid1 = 0, valid2 = 0;
	static ap_uint<32> left0, right0, left1, right1, left2, right2;
	static ap_uint<3> index = 0;
	static bool newData = false;
	static ap_uint<32> mergeL, mergeR;
	static bool usable, hit0, hit1, hit2, lowL0, lowL1, lowL2, highR0, highR1, highR2, rank01, rank02, rank12;

	switch (state)
	{
	case IDLE:
		if (!txEng2sack_clear.empty())
		{
			ap_uint<16> id = txEng2sack_clear.read();
			sack_table[id] = clearedEntry();
		}
		else if (!rxEng2sack_req.empty())
		{
			rxEng2sack_req.read(req);
			if (req.init)
			{
				sack_table[req.sessionID] = clearedEntry();
			}
			else
			{
				sackEntry entry = sack_table[req.sessionID];
				valid0 = entry.valid[0]; valid1 = entry.valid[1]; valid2 = entry.valid[2];
				left0 = entry.left0; right0 = entry.right0;
				left1 = entry.left1; right1 = entry.right1;
				left2 = entry.left2; right2 = entry.right2;
				trimBlock(valid0, left0, right0, req.ack);
				trimBlock(valid1, left1, right1, req.ack);
				trimBlock(valid2, left2, right2, req.ack);
				index = 0;
				newData = false;
				state = (req.count != 0) ? MERGE_CMP : COALESCE;
			}
		}
		break;
	case MERGE_CMP:
	{
		ap_uint<64> block = req.blocks(64*index+63, 64*index);
		ap_uint<32> l = block(31, 0);
		ap_uint<32> r = block(63, 32);
		ap_uint<32> lc = seqBefore(l, req.ack) ? req.ack : l;
		usable = seqAfter(r, l) && seqAfter(r, req.ack) && ap_uint<32>(r - req.ack) <= BUFFER_SIZE;
		mergeL = lc;
		mergeR = r;
		hit0 = touches(lc, r, valid0, left0, right0);
		hit1 = touches(lc, r, valid1, left1, right1);
		hit2 = touches(lc, r, valid2, left2, right2);
		lowL0 = seqBefore(lc, left0); lowL1 = seqBefore(lc, left1); lowL2 = seqBefore(lc, left2);
		highR0 = seqAfter(r, right0); highR1 = seqAfter(r, right1); highR2 = seqAfter(r, right2);
		rank01 = seqBefore(right0, right1); rank02 = seqBefore(right0, right2); rank12 = seqBefore(right1, right2);
		state = MERGE_APPLY;
		break;
	}
	case MERGE_APPLY:
		if (usable)
		{
			if (hit0)
			{
				if (lowL0) { left0 = mergeL; newData = true; }
				if (highR0) { right0 = mergeR; newData = true; }
			}
			else if (hit1)
			{
				if (lowL1) { left1 = mergeL; newData = true; }
				if (highR1) { right1 = mergeR; newData = true; }
			}
			else if (hit2)
			{
				if (lowL2) { left2 = mergeL; newData = true; }
				if (highR2) { right2 = mergeR; newData = true; }
			}
			else
			{
				newData = true;
				if (!valid0)
				{
					valid0 = 1; left0 = mergeL; right0 = mergeR;
				}
				else if (!valid1)
				{
					valid1 = 1; left1 = mergeL; right1 = mergeR;
				}
				else if (!valid2)
				{
					valid2 = 1; left2 = mergeL; right2 = mergeR;
				}
				else if (rank01 && rank02)
				{
					left0 = mergeL; right0 = mergeR;
				}
				else if (rank12)
				{
					left1 = mergeL; right1 = mergeR;
				}
				else
				{
					left2 = mergeL; right2 = mergeR;
				}
			}
		}
		index++;
		state = (index == req.count) ? COALESCE : MERGE_CMP;
		break;
	case COALESCE:
	{
		bool t01 = valid0 && touches(left1, right1, valid1, left0, right0);
		bool t02 = valid0 && touches(left2, right2, valid2, left0, right0);
		bool t12 = valid1 && touches(left2, right2, valid2, left1, right1);
		bool l01 = seqBefore(left1, left0), l02 = seqBefore(left2, left0), l12 = seqBefore(left2, left1);
		bool r01 = seqAfter(right1, right0), r02 = seqAfter(right2, right0), r12 = seqAfter(right2, right1);
		ap_uint<32> min01 = l01 ? left1 : left0;
		ap_uint<32> max01 = r01 ? right1 : right0;
		ap_uint<32> min02 = l02 ? left2 : left0;
		ap_uint<32> max02 = r02 ? right2 : right0;
		ap_uint<32> min12 = l12 ? left2 : left1;
		ap_uint<32> max12 = r12 ? right2 : right1;
		bool lowest2 = l02 && l12;
		bool highest2 = r02 && r12;
		if (t01 && (t02 || t12))
		{
			left0 = lowest2 ? left2 : min01;
			right0 = highest2 ? right2 : max01;
			valid1 = 0; valid2 = 0;
		}
		else if (t01)
		{
			left0 = min01; right0 = max01; valid1 = 0;
		}
		else if (t02)
		{
			left0 = min02; right0 = max02; valid2 = 0;
		}
		else if (t12)
		{
			left1 = min12; right1 = max12; valid2 = 0;
		}
		state = WRITE;
		break;
	}
	case WRITE:
	{
		sackEntry entry;
		entry.valid = (valid2, valid1, valid0);
		entry.left0 = left0; entry.right0 = right0;
		entry.left1 = left1; entry.right1 = right1;
		entry.left2 = left2; entry.right2 = right2;
		sack_table[req.sessionID] = entry;
		ap_uint<WINDOW_BITS> len0 = valid0 ? ap_uint<WINDOW_BITS>(right0 - left0) : ap_uint<WINDOW_BITS>(0);
		ap_uint<WINDOW_BITS> len1 = valid1 ? ap_uint<WINDOW_BITS>(right1 - left1) : ap_uint<WINDOW_BITS>(0);
		ap_uint<WINDOW_BITS> len2 = valid2 ? ap_uint<WINDOW_BITS>(right2 - left2) : ap_uint<WINDOW_BITS>(0);
		ap_uint<2> distinct = valid0 + valid1 + valid2;
		sack2rxEng_rsp.write(sackReply(entry.valid, left0, right0, left1, right1, left2, right2, len0 + len1 + len2, distinct, newData));
		state = IDLE;
		break;
	}
	}
}
