/*
 * Copyright (c) 2019, Systems Group, ETH Zurich
 * Copyright (c) 2016, Xilinx, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "rx_sar_table.hpp"

using namespace hls;

/** @ingroup rx_sar_table
 * 	This data structure stores the RX(receiving) sliding window
 *  and handles concurrent access from the @ref rx_engine, @ref rx_app_if
 *  and @ref tx_engine
 *  @param[in]		rxEng2rxSar_upd_req
 *  @param[in]		rxApp2rxSar_upd_req
 *  @param[in]		txEng2rxSar_upd_req
 *  @param[out]		rxSar2rxEng_upd_rsp
 *  @param[out]		rxSar2rxApp_upd_rsp
 *  @param[out]		rxSar2txEng_upd_rsp
 */
void rx_sar_table(	stream<rxSarRecvd>&			rxEng2rxSar_upd_req,
					stream<rxSarAppd>&			rxApp2rxSar_upd_req,
					stream<ap_uint<16> >&		txEng2rxSar_req, //read only
					stream<rxSarEntry>&			rxSar2rxEng_upd_rsp,
					stream<rxSarAppd>&			rxSar2rxApp_upd_rsp,
					stream<rxSarReply>&			rxSar2txEng_rsp
#ifdef V80_RX_MEM_NO_SPLIT
					,stream<rxSarAppd>&			rxApp2rxSar_release
					,stream<ackRequest>&		rxSar2ackSched_req
#endif
#ifdef V80_ZC
					,stream<appRxRelease>&		rxApp2rxSar_zcRel
					,ap_uint<1>					zcRx
					,ap_uint<32>&				zcRelBad
#endif
#if (RX_DDR_BYPASS)
                    ,ap_uint<16>                rxbuffer_data_count
                    ,ap_uint<16>                rxbuffer_max_data_count
#endif
)

{

	static rxSarEntry rx_table[MAX_SESSIONS];

#pragma HLS PIPELINE II=1

#pragma HLS bind_storage variable=rx_table type=RAM_1P impl=BRAM
#pragma HLS DEPENDENCE variable=rx_table inter false
#ifdef V80_RX_MEM_NO_SPLIT
	static ap_uint<WINDOW_BITS> rx_adv_edge[MAX_SESSIONS];
#pragma HLS bind_storage variable=rx_adv_edge type=RAM_1P impl=BRAM
#pragma HLS DEPENDENCE variable=rx_adv_edge inter false
#endif
#ifdef V80_ZC
	static ap_uint<2> zc_flags[MAX_SESSIONS];
#pragma HLS bind_storage variable=zc_flags type=RAM_T2P impl=BRAM
#pragma HLS DEPENDENCE variable=zc_flags inter false
#pragma HLS reset variable=zc_flags off
	static ap_uint<WINDOW_BITS> zc_cur[MAX_SESSIONS];
#pragma HLS bind_storage variable=zc_cur type=RAM_2P impl=BRAM
#pragma HLS DEPENDENCE variable=zc_cur inter false
#pragma HLS reset variable=zc_cur off
	static ap_uint<32> zc_bad = 0;
	static bool zc_hold = false;
	zcRelBad = zc_bad;
	bool zc_cw = false;
	ap_uint<16> zc_ca = 0;
	ap_uint<WINDOW_BITS> zc_cv = 0;
	bool zc_tx = !txEng2rxSar_req.empty();
	bool zc_app_ne = !rxApp2rxSar_upd_req.empty();
	bool zc_eng_ne = !rxEng2rxSar_upd_req.empty();
	bool zc_rel_ne = !rxApp2rxSar_release.empty();
	bool zc_zr_ne = !rxApp2rxSar_zcRel.empty();
	bool zc_free = !zc_tx & !zc_app_ne;
	bool zc_open = !zc_hold;
	bool zc_app = !zc_tx & zc_app_ne;
	bool zc_eng = zc_free & zc_open & zc_eng_ne;
	bool zc_rel = zc_free & !zc_eng & zc_rel_ne;
	bool zc_zr = zc_free & zc_open & !zc_eng_ne & !zc_rel_ne & zc_zr_ne;
	rxSarRecvd in_recvd;
	appRxRelease zrel;
	if (zc_eng)
	{
		in_recvd = rxEng2rxSar_upd_req.read();
	}
	if (zc_zr)
	{
		zrel = rxApp2rxSar_zcRel.read();
	}
	zc_hold = (zc_eng & in_recvd.write & in_recvd.init) | (zc_zr & zrel.arm);
#endif

	// Read only access from the Tx Engine
#ifdef V80_ZC
	if(zc_tx)
#else
	if(!txEng2rxSar_req.empty())
#endif
	{
		ap_uint<16> addr = txEng2rxSar_req.read();
		rxSarEntry entry = rx_table[addr];
		rxSarReply reply (entry);

#if (RX_DDR_BYPASS)
      ap_uint<WINDOW_BITS> actualWindowSize = 0;

      if ((rxbuffer_max_data_count != 0) && (rxbuffer_data_count < rxbuffer_max_data_count))
      {
        ap_uint<16> fifo_free_entries = rxbuffer_max_data_count - rxbuffer_data_count;
#ifdef V80_RX_WINDOW_MAX
        // V80 nvmet: every session may take 1/2^V80_RX_WINDOW_SHIFT of the free FIFO bytes above a reserve of
        // V80_RX_WINDOW_RESERVE entries, up to V80_RX_WINDOW_MAX (< 2^WINDOW_BITS); the fixed 8 x MSS below
        // capped one connection at ~4.5 GB/s. rx_engine drops every segment while the FIFO has <= 375 free
        // entries and this TOE cannot reassemble, so a drop costs the sender go-back-N and RTOs: with at most
        // 2^SHIFT sessions the windows together never reach into the reserve, so the FIFO stays above it.
        ap_uint<16> usable_entries = (fifo_free_entries > (ap_uint<16>)V80_RX_WINDOW_RESERVE) ?
                                     (ap_uint<16>)(fifo_free_entries - V80_RX_WINDOW_RESERVE) : (ap_uint<16>)0;
        ap_uint<16 + DATA_KEEP_BITS> free_bytes = ((ap_uint<16 + DATA_KEEP_BITS>)usable_entries) << DATA_KEEP_BITS;
        ap_uint<16 + DATA_KEEP_BITS> share = free_bytes >> V80_RX_WINDOW_SHIFT;
        actualWindowSize = (share < (ap_uint<16 + DATA_KEEP_BITS>)V80_RX_WINDOW_MAX) ? (ap_uint<WINDOW_BITS>)share
                                                                                   : (ap_uint<WINDOW_BITS>)V80_RX_WINDOW_MAX;
#else
        ap_uint<18> fifo_free_entries_wide = fifo_free_entries;
        ap_uint<18> rxbuffer_max_data_count_wide = rxbuffer_max_data_count;
        ap_uint<WINDOW_BITS> target_window = 0;

        if ((fifo_free_entries_wide << 2) <= rxbuffer_max_data_count_wide)
        {
          target_window = 0;
        }
        else if ((fifo_free_entries_wide << 1) <= rxbuffer_max_data_count_wide)
        {
          target_window = MSS;
        }
        else
        {
          target_window = 8 * MSS;
        }

        ap_uint<16 + DATA_KEEP_BITS> fifo_free_bytes =
            ((ap_uint<16 + DATA_KEEP_BITS>)fifo_free_entries) << DATA_KEEP_BITS;
        actualWindowSize = (fifo_free_bytes < target_window) ?
                           (ap_uint<WINDOW_BITS>)fifo_free_bytes : target_window;
#endif
      }

#if (WINDOW_SCALE)
#ifdef V80_RX_WINDOW_MAX
      // a peer without window scaling sees at most 64 KiB - 1 (not the value truncated to 16 bits)
      if (entry.win_shift == 0 && actualWindowSize > 0xFFFF) actualWindowSize = 0xFFFF;
#endif
      reply.windowSize = actualWindowSize >> entry.win_shift;
#else
      reply.windowSize = actualWindowSize(15, 0);
#endif

#else  // !(RX_DDR_BYPASS)		
#ifdef V80_RX_MEM_NO_SPLIT
				entry.appd = entry.released;
#endif
#ifdef V80_ZC
				if (zc_flags[addr][0])
				{
					entry.appd = (ap_uint<WINDOW_BITS>)entry.recvd + 1;
				}
#endif
		//Pre-calculated usedLength, windowSize to improve timing in metaLoader
#if (WINDOW_SCALE)
				ap_uint<WINDOW_BITS> actualWindowSize = (entry.appd - ((ap_uint<WINDOW_BITS>)entry.recvd)) - 1; // This works even for wrap around
				reply.windowSize = actualWindowSize >> entry.win_shift;
#ifdef V80_RX_MEM_NO_SPLIT
				rx_adv_edge[addr] = (ap_uint<WINDOW_BITS>)entry.recvd + (((ap_uint<WINDOW_BITS>)reply.windowSize) << entry.win_shift);
#endif
#else
				//This works even for wrap around
				reply.windowSize = (entry.appd - ((ap_uint<16>)entry.recvd)) - 1; // This works even for wrap around
#ifdef V80_RX_MEM_NO_SPLIT
				rx_adv_edge[addr] = (ap_uint<WINDOW_BITS>)entry.recvd + (ap_uint<WINDOW_BITS>)reply.windowSize;
#endif
#endif

#endif // RX_DDR_BYPASS

		rxSar2txEng_rsp.write(reply);
	}
	// Read or Write access from the Rx App I/F to update the application pointer
#ifdef V80_ZC
	else if(zc_app)
#else
	else if(!rxApp2rxSar_upd_req.empty())
#endif
	{
		rxSarAppd in_appd = rxApp2rxSar_upd_req.read();
		if(in_appd.write)
		{
			rx_table[in_appd.sessionID].appd = in_appd.appd;
		}
		else
		{
#ifdef V80_ZC
			rxSar2rxApp_upd_rsp.write(rxSarAppd(in_appd.sessionID, rx_table[in_appd.sessionID].appd, rx_table[in_appd.sessionID].base, 0));
#else
			rxSar2rxApp_upd_rsp.write(rxSarAppd(in_appd.sessionID, rx_table[in_appd.sessionID].appd));
#endif
		}
	}
	// Read or Write access from the Rx Engine
#ifdef V80_ZC
	else if(zc_eng)
#else
	else if(!rxEng2rxSar_upd_req.empty())
#endif
	{
#ifndef V80_ZC
		rxSarRecvd in_recvd = rxEng2rxSar_upd_req.read();
#endif
		if (in_recvd.write)
		{
			rx_table[in_recvd.sessionID].recvd = in_recvd.recvd;
			rx_table[in_recvd.sessionID].head = in_recvd.head;
			rx_table[in_recvd.sessionID].offset = in_recvd.offset;
			rx_table[in_recvd.sessionID].gap = in_recvd.gap;
			if (in_recvd.init)
			{
				rx_table[in_recvd.sessionID].sack_ok = in_recvd.sack_ok;
				rx_table[in_recvd.sessionID].appd = in_recvd.recvd;
#ifdef V80_RX_MEM_NO_SPLIT
				rx_table[in_recvd.sessionID].released = in_recvd.recvd;
				rx_adv_edge[in_recvd.sessionID] = (ap_uint<WINDOW_BITS>)in_recvd.recvd - 1;
#endif
#ifdef V80_ZC
				rx_table[in_recvd.sessionID].base = in_recvd.recvd;
				zc_flags[in_recvd.sessionID] = (ap_uint<1>(1), ap_uint<1>(zcRx & zc_flags[in_recvd.sessionID][1]));
				zc_cw = true;
				zc_ca = in_recvd.sessionID;
				zc_cv = 0;
#endif
#if (WINDOW_SCALE)
				rx_table[in_recvd.sessionID].win_shift = in_recvd.win_shift;
#endif
			}
		}
		else
		{
#ifdef V80_ZC
			rxSarEntry eng_entry = rx_table[in_recvd.sessionID];
			if (zc_flags[in_recvd.sessionID][0])
			{
				eng_entry.released = (ap_uint<WINDOW_BITS>)eng_entry.head + 1;
			}
			rxSar2rxEng_upd_rsp.write(eng_entry);
#else
			rxSar2rxEng_upd_rsp.write(rx_table[in_recvd.sessionID]);
#endif
		}
	}
#ifdef V80_RX_MEM_NO_SPLIT
#ifdef V80_ZC
	else if(zc_rel)
#else
	else if(!rxApp2rxSar_release.empty())
#endif
	{
		rxSarAppd rel = rxApp2rxSar_release.read();
		ap_uint<WINDOW_BITS> recvd = (ap_uint<WINDOW_BITS>)rx_table[rel.sessionID].recvd;
		ap_uint<WINDOW_BITS> advertised = rx_adv_edge[rel.sessionID] - recvd;
		ap_uint<WINDOW_BITS> opened = rel.appd - recvd - 1;
		rx_table[rel.sessionID].released = rel.appd;
#ifdef V80_ZC
		zc_cw = true;
		zc_ca = rel.sessionID;
		zc_cv = rel.appd - rx_table[rel.sessionID].base;
#endif
		if (!advertised[WINDOW_BITS-1] && opened >= MSS && opened >= (ap_uint<WINDOW_BITS+1>(advertised) << 1))
		{
			rxSar2ackSched_req.write(ackRequest(rel.sessionID, true));
		}
	}
#endif
#ifdef V80_ZC
	else if(zc_zr)
	{
		rxSarEntry zentry = rx_table[zrel.sessionID];
		ap_uint<WINDOW_BITS> zrecvd = (ap_uint<WINDOW_BITS>)zentry.recvd;
		ap_uint<WINDOW_BITS> zcur = zc_cur[zrel.sessionID];
		ap_uint<WINDOW_BITS> zlim = zentry.appd - zentry.base;
		ap_uint<WINDOW_BITS> zptr = zrel.ptr(WINDOW_BITS-1, 0);
		ap_uint<WINDOW_BITS> znew = zentry.appd;
		bool zopen = false;
		ap_uint<2> zflags = zc_flags[zrel.sessionID];
		if (zrel.arm)
		{
			if (zflags[0])
			{
				zc_flags[zrel.sessionID] = (zflags[1], ap_uint<1>(0));
				zopen = true;
			}
			else
			{
				zc_flags[zrel.sessionID] = (ap_uint<1>(0), zflags[0]);
			}
		}
		else if ((ap_uint<WINDOW_BITS>)(zptr - zcur) <= (ap_uint<WINDOW_BITS>)(zlim - zcur))
		{
			znew = zptr + zentry.base;
			zopen = true;
		}
		else
		{
			zc_bad++;
		}
		if (zopen)
		{
			ap_uint<WINDOW_BITS> zadvertised = rx_adv_edge[zrel.sessionID] - zrecvd;
			ap_uint<WINDOW_BITS> zopened = znew - zrecvd - 1;
			rx_table[zrel.sessionID].released = znew;
			zc_cw = true;
			zc_ca = zrel.sessionID;
			zc_cv = znew - zentry.base;
			if (!zadvertised[WINDOW_BITS-1] && zopened >= MSS && zopened >= (ap_uint<WINDOW_BITS+1>(zadvertised) << 1))
			{
				rxSar2ackSched_req.write(ackRequest(zrel.sessionID, true));
			}
		}
	}
#endif
#ifdef V80_ZC
	if (zc_cw)
	{
		zc_cur[zc_ca] = zc_cv;
	}
#endif
}
