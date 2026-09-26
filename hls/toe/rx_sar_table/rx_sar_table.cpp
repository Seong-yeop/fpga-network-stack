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

	// Read only access from the Tx Engine
	if(!txEng2rxSar_req.empty())
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
		//Pre-calculated usedLength, windowSize to improve timing in metaLoader
#if (WINDOW_SCALE)
				ap_uint<WINDOW_BITS> actualWindowSize = (entry.appd - ((ap_uint<WINDOW_BITS>)entry.recvd)) - 1; // This works even for wrap around
				reply.windowSize = actualWindowSize >> entry.win_shift;
#else
				//This works even for wrap around
				reply.windowSize = (entry.appd - ((ap_uint<16>)entry.recvd)) - 1; // This works even for wrap around
#endif

#endif // RX_DDR_BYPASS

		rxSar2txEng_rsp.write(reply);
	}
	// Read or Write access from the Rx App I/F to update the application pointer
	else if(!rxApp2rxSar_upd_req.empty())
	{
		rxSarAppd in_appd = rxApp2rxSar_upd_req.read();
		if(in_appd.write)
		{
			rx_table[in_appd.sessionID].appd = in_appd.appd;
		}
		else
		{
			rxSar2rxApp_upd_rsp.write(rxSarAppd(in_appd.sessionID, rx_table[in_appd.sessionID].appd));
		}
	}
	// Read or Write access from the Rx Engine
	else if(!rxEng2rxSar_upd_req.empty())
	{
		rxSarRecvd in_recvd = rxEng2rxSar_upd_req.read();
		if (in_recvd.write)
		{
			rx_table[in_recvd.sessionID].recvd = in_recvd.recvd;
			rx_table[in_recvd.sessionID].head = in_recvd.head;
			rx_table[in_recvd.sessionID].offset = in_recvd.offset;
			rx_table[in_recvd.sessionID].gap = in_recvd.gap;
			if (in_recvd.init)
			{
				rx_table[in_recvd.sessionID].appd = in_recvd.recvd;
#ifdef V80_RX_MEM_NO_SPLIT
				rx_table[in_recvd.sessionID].released = in_recvd.recvd;
#endif
#if (WINDOW_SCALE)
				rx_table[in_recvd.sessionID].win_shift = in_recvd.win_shift;
#endif
			}
		}
		else
		{
			rxSar2rxEng_upd_rsp.write(rx_table[in_recvd.sessionID]);
		}
	}
#ifdef V80_RX_MEM_NO_SPLIT
	else if(!rxApp2rxSar_release.empty())
	{
		rxSarAppd rel = rxApp2rxSar_release.read();
		rx_table[rel.sessionID].released = rel.appd;
	}
#endif
}
