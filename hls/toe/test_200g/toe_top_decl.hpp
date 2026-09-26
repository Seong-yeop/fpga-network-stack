#pragma once
#include "../toe.hpp"
#include "../session_lookup_controller/session_lookup_controller.hpp"
using namespace hls;
#ifdef V80_RX_DEBUG
static ap_uint<32> v80_dbg_rx_header;
static ap_uint<64> v80_dbg_rx_meta;
static ap_uint<192> v80_dbg_rx_fsm;
static ap_uint<192> v80_dbg_rx_gap;
#endif
void toe(	// Data & Memory Interface
			stream<ap_axiu<DATA_WIDTH, 0, 0, 0> >&						ipRxData,
#if !(RX_DDR_BYPASS)
			stream<mmStatus>&						rxBufferWriteStatus,
#endif
			stream<mmStatus>&						txBufferWriteStatus,
			stream<ap_axiu<DATA_WIDTH, 0, 0, 0> >&						rxBufferReadData,
			stream<ap_axiu<DATA_WIDTH, 0, 0, 0> >&						txBufferReadData,
			stream<ap_axiu<DATA_WIDTH, 0, 0, 0> >&						ipTxData,
#if !(RX_DDR_BYPASS)
			stream<mmCmd>&							rxBufferWriteCmd,
			stream<mmCmd>&							rxBufferReadCmd,
#endif
			stream<mmCmd>&							txBufferWriteCmd,
			stream<mmCmd>&							txBufferReadCmd,
			stream<ap_axiu<DATA_WIDTH, 0, 0, 0> >&						rxBufferWriteData,
			stream<ap_axiu<DATA_WIDTH, 0, 0, 0> >&						txBufferWriteData,

			// SmartCam Interface
			stream<rtlSessionLookupReply>&			sessionLookup_rsp,
			stream<rtlSessionUpdateReply>&			sessionUpdate_rsp,
			stream<rtlSessionLookupRequest>&		sessionLookup_req,
			stream<rtlSessionUpdateRequest>&		sessionUpdate_req,
			// Application Interface
			stream<ap_uint<16> >&					listenPortReq,
			// This is disabled for the time being, due to complexity concerns
			//stream<ap_uint<16> >&					appClosePortIn,
			stream<appReadRequest>&					rxDataReq,
			stream<ipTuple>&						openConnReq,
			stream<ap_uint<16> >&					closeConnReq,
			stream<appTxMeta>&					   txDataReqMeta,
			stream<ap_axiu<DATA_WIDTH, 0, 0, 0> >&						txDataReq,

			stream<bool>&							listenPortRsp,
			stream<appNotification>&				notification,
			stream<ap_uint<16> >&					rxDataRspMeta,
			stream<ap_axiu<DATA_WIDTH, 0, 0, 0> >&						rxDataRsp,
			stream<openStatus>&						openConnRsp,
			stream<appTxRsp>&					txDataRsp,
#if RX_DDR_BYPASS
			// Data counts for external FIFO
			ap_uint<16>						axis_data_count,
			ap_uint<16>						axis_max_data_count,
#endif
			//IP Address Input
			ap_uint<32>								myIpAddress,
			//statistic
			ap_uint<16>&							regSessionCount
#ifdef V80_RUNTIME_MSS
, ap_uint<16> maxSegmentSize = TCP_MSS
#endif
#ifdef V80_RX_DEBUG
, ap_uint<32>& dbgRxHeader = v80_dbg_rx_header, ap_uint<64>& dbgRxMeta = v80_dbg_rx_meta, ap_uint<192>& dbgRxFsm = v80_dbg_rx_fsm, ap_uint<192>& dbgRxGap = v80_dbg_rx_gap
#endif
)
;
