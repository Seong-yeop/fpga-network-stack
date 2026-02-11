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
#include "toe.hpp"
#include "dummy_memory.hpp"
#include "session_lookup_controller/session_lookup_controller.hpp"
#include <map>
#include <string>
#include <vector>
#include <cstring>
#include <iomanip>

#define noOfTxSessions 0 // Number of Tx Sessions to open for testing
#define totalSimCycles 30000

using namespace std;

// DDR Write Record for verification
struct DdrWriteRecord {
    ap_uint<16> sessionId;
    ap_uint<WINDOW_BITS> addr;  // Extended for WINDOW_SCALE support
    uint32_t length;
    std::vector<uint8_t> data;
};
std::vector<DdrWriteRecord> ddrWriteRecords;

// Expected TCP payload from input file
struct ExpectedPayload {
    std::vector<uint8_t> data;
    uint16_t packetIndex;
};
std::vector<ExpectedPayload> expectedPayloads;

// Received payload from rxDataOut (application output)
struct ReceivedPayload {
    std::vector<uint8_t> data;
};
std::vector<ReceivedPayload> receivedPayloads;
static ReceivedPayload currentReceivedPayload;  // For accumulating data across words

uint32_t cycleCounter;
unsigned int	simCycleCounter		= 0;

void sessionLookupStub(stream<rtlSessionLookupRequest>& lup_req, stream<rtlSessionLookupReply>& lup_rsp,
						stream<rtlSessionUpdateRequest>& upd_req, stream<rtlSessionUpdateReply>& upd_rsp) {
						//stream<ap_uint<14> >& new_id, stream<ap_uint<14> >& fin_id)
	static map<threeTupleInternal, ap_uint<14> > lookupTable;

	rtlSessionLookupRequest request;
	rtlSessionUpdateRequest update;

	map<threeTupleInternal, ap_uint<14> >::const_iterator findPos;

	if (!lup_req.empty()) {
		lup_req.read(request);
		findPos = lookupTable.find(request.key);
		if (findPos != lookupTable.end()) //hit
			lup_rsp.write(rtlSessionLookupReply(true, findPos->second, request.source));
		else
			lup_rsp.write(rtlSessionLookupReply(false, request.source));
	}

	if (!upd_req.empty()) {	//TODO what if element does not exist
		upd_req.read(update);
		if (update.op == INSERT) {	//Is there a check if it already exists?
			// Read free id
			//new_id.read(update.value);
			lookupTable[update.key] = update.value;
			upd_rsp.write(rtlSessionUpdateReply(update.value, INSERT, update.source));

		}
		else {	// DELETE
			//fin_id.write(update.value);
			lookupTable.erase(update.key);
			upd_rsp.write(rtlSessionUpdateReply(update.value, DELETE, update.source));
		}
	}
}

// Use Dummy Memory
template <int WIDTH>
void simulateRx(dummyMemory<WIDTH>* memory, stream<mmCmd>& WriteCmdFifo,  stream<mmStatus>& WriteStatusFifo, stream<mmCmd>& ReadCmdFifo,
					stream<net_axis<WIDTH> >& BufferIn, stream<net_axis<WIDTH> >& BufferOut) {
	mmCmd cmd;
	mmStatus status;
	net_axis<WIDTH> inWord = net_axis<WIDTH>(0, 0, 0);
	net_axis<WIDTH> outWord = net_axis<WIDTH>(0, 0, 0);
	static uint32_t rxMemCounter = 0;
	static uint32_t rxMemCounterRd = 0;

	static bool stx_write = false;
	// stx_read removed - read commands now processed atomically

	static ap_uint<16> wrBufferWriteCounter = 0;
	static ap_uint<16> wrBufferReadCounter = 0;

	// For verification: track current write record
	static DdrWriteRecord currentRecord;

	if (!WriteCmdFifo.empty() && !stx_write) {
		WriteCmdFifo.read(cmd);
		memory->setWriteCmd(cmd);
		// Extract session ID and address matching dummyMemory format:
		// cmd.saddr[29:WINDOW_BITS] = sessionID, cmd.saddr[WINDOW_BITS-1:0] = offset
		ap_uint<16> sessId = cmd.saddr(29, WINDOW_BITS);
		ap_uint<WINDOW_BITS> addr = cmd.saddr(WINDOW_BITS-1, 0);
		cerr << "[RX Write Cmd] sessionID=0x" << hex << (uint16_t)sessId
		     << " addr=0x" << (uint32_t)addr
		     << " len=" << dec << (uint16_t)cmd.bbt << endl;
		wrBufferWriteCounter = cmd.bbt;
		stx_write = true;

		// Start new record
		currentRecord.sessionId = sessId;
		currentRecord.addr = addr;
		currentRecord.length = cmd.bbt;
		currentRecord.data.clear();
	}
	else if (!BufferIn.empty() && stx_write) {
		BufferIn.read(inWord);
		memory->writeWord(inWord);

		// Record the written bytes
		for (int i = 0; i < (WIDTH/8); i++) {
			if (inWord.keep[i]) {
				currentRecord.data.push_back((uint8_t)(inWord.data((i*8)+7, i*8)));
			}
		}

		if (wrBufferWriteCounter <= (WIDTH/8)) {
			stx_write = false;
			status.okay = 1;
			WriteStatusFifo.write(status);

			// Save completed record
			ddrWriteRecords.push_back(currentRecord);
			cerr << "[RX Write Complete] sessionID=0x" << hex << (uint16_t)currentRecord.sessionId
			     << " addr=0x" << (uint32_t)currentRecord.addr
			     << " len=" << dec << currentRecord.data.size() << " bytes" << endl;
		}
		else
			wrBufferWriteCounter -= (WIDTH/8);
	}
	// Process read commands atomically (all data for a command before accepting new command)
	// This matches real HW behavior where DMA transfers complete before the next one starts.
	// Without this, rxAppMemDataRead may read doubleAccess before corresponding data is ready,
	// causing FIFO desynchronization.
	if (!ReadCmdFifo.empty()) {
		ReadCmdFifo.read(cmd);
		memory->setReadCmd(cmd);
		ap_uint<16> readSessId = cmd.saddr(29, WINDOW_BITS);
		ap_uint<WINDOW_BITS> readOffset = cmd.saddr(WINDOW_BITS-1, 0);
		cerr << "[simulateRx READ CMD] sessionID=0x" << hex << (uint16_t)readSessId
		     << " addr=0x" << (uint32_t)readOffset
		     << " len=" << dec << (uint16_t)cmd.bbt << endl;
		wrBufferReadCounter = cmd.bbt;

		// Process all words for this command atomically
		do {
			memory->readWord(outWord);
			BufferOut.write(outWord);
			int keepLen = 0;
			for (int i = 0; i < (WIDTH/8); i++) {
				if (outWord.keep[i]) keepLen++;
			}
			// cerr << "[simulateRx READ DATA] keepLen=" << dec << keepLen
			//      << " last=" << (int)outWord.last
			//      << " remaining=" << (int)wrBufferReadCounter << endl;
			rxMemCounterRd++;
			if (wrBufferReadCounter <= (WIDTH/8))
				wrBufferReadCounter = 0;
			else
				wrBufferReadCounter -= (WIDTH/8);
		} while (wrBufferReadCounter > 0);
	}
}



template <int WIDTH>
void simulateTx(dummyMemory<WIDTH>* memory, stream<mmCmd>& WriteCmdFifo,  stream<mmStatus>& WriteStatusFifo, stream<mmCmd>& ReadCmdFifo,
					stream<net_axis<WIDTH> >& BufferIn, stream<net_axis<WIDTH> >& BufferOut) {
	mmCmd cmd;
	mmStatus status;
	net_axis<WIDTH> inWord;
	net_axis<WIDTH> outWord;

	static bool stx_write = false;
	static bool stx_read = false;

	static bool stx_readCmd = false;

	if (!WriteCmdFifo.empty() && !stx_write) {
		WriteCmdFifo.read(cmd);
		//cerr << "WR: " << dec << cycleCounter << hex << " - " << cmd.saddr << " - " << cmd.bbt << endl;
		memory->setWriteCmd(cmd);
		stx_write = true;
	}
	else if (!BufferIn.empty() && stx_write) {
		BufferIn.read(inWord);
		//cerr << "Data: " << dec << cycleCounter << hex << inWord.data << " - " << inWord.keep << " - " << inWord.last << endl;
		memory->writeWord(inWord);
		if (inWord.last) {
			//fake_txBuffer.write(inWord); // RT hack
			stx_write = false;
			status.okay = 1;
			WriteStatusFifo.write(status);
		}
	}
	if (!ReadCmdFifo.empty() && !stx_read) {
		ReadCmdFifo.read(cmd);
		//cerr << "RD: " << cmd.saddr << " - " << cmd.bbt << endl;
		memory->setReadCmd(cmd);
		stx_read = true;
	}
	else if(stx_read) {
		memory->readWord(outWord);
		//cerr << inWord.data << " " << inWord.last << " - ";
		BufferOut.write(outWord);
		if (outWord.last)
			stx_read = false;
	}
}

void iperf(	stream<ap_uint<16> >& listenPort, stream<bool>& listenPortStatus,
			// This is disabled for the time being, because it adds complexity/potential issues
			//stream<ap_uint<16> >& closePort,
			stream<appNotification>& notifications, stream<appReadRequest>& readRequest,
			stream<ap_uint<16> >& rxMetaData, stream<net_axis<64> >& rxData, stream<net_axis<64> >& rxDataOut,
			stream<ipTuple>& openConnection, stream<openStatus>& openConStatus,
			stream<ap_uint<16> >& closeConnection, vector<ap_uint<16> > &txSessionIDs) {

	static bool listenDone = false;
	static bool runningExperiment = false;
	static ap_uint<1> listenFsm = 0;

	openStatus newConStatus;
	appNotification notification;
	ipTuple tuple;

	// Open Port 5001
	if (!listenDone) {
		switch (listenFsm) {
		case 0:
			listenPort.write(0x57);
			listenFsm++;
			break;
		case 1:
			if (!listenPortStatus.empty()) {
				listenPortStatus.read(listenDone);
				listenFsm++;
			}
			break;
		}
	}

	net_axis<64> transmitWord;
	// In case we are connecting back
	if (!openConStatus.empty()) {
		openStatus tempStatus = openConStatus.read();
		if(tempStatus.success)
			txSessionIDs.push_back(tempStatus.sessionID);
	}

	if (!notifications.empty())	{
		notifications.read(notification);

		if (notification.length != 0)
			readRequest.write(appReadRequest(notification.sessionID, notification.length));
		else // closed
			runningExperiment = false;
	}

	enum consumeFsmStateType {WAIT_PKG, CONSUME, HEADER_2, HEADER_3};
	static consumeFsmStateType  serverFsmState = WAIT_PKG;
	ap_uint<16> sessionID;
	net_axis<64> currWord;
	currWord.last = 0;
	static bool dualTest = false;
	static ap_uint<32> mAmount = 0;

	switch (serverFsmState)	{
	case WAIT_PKG:
		if (!rxMetaData.empty() && !rxData.empty())	{
			rxMetaData.read(sessionID);
			rxData.read(currWord);
			rxDataOut.write(currWord);
			if (!runningExperiment) {
				if (currWord.data(31, 0) == 0x00000080) // Dual test
					dualTest = true;
				else
					dualTest = false;

				runningExperiment = true;
				serverFsmState = HEADER_2;
			}
			else
				serverFsmState = CONSUME;
		}
		break;
	case HEADER_2:
		if (!rxData.empty()) {
			rxData.read(currWord);
			rxDataOut.write(currWord);
			if (dualTest) {
				tuple.ip_address = 0x0a010101;
				tuple.ip_port = currWord.data(31, 16);
				openConnection.write(tuple);
			}
			serverFsmState = HEADER_3;
		}
		break;
	case HEADER_3:
		if (!rxData.empty()) {
			rxData.read(currWord);
			rxDataOut.write(currWord);
			mAmount = currWord.data(63, 32);
			serverFsmState = CONSUME;
		}
		break;
	case CONSUME:
		if (!rxData.empty()) {
			rxData.read(currWord);
			rxDataOut.write(currWord);
		}
		break;
	}
	if (currWord.last == 1)
		serverFsmState = WAIT_PKG;
}

string decodeApUint64(ap_uint<64> inputNumber) {
	string 				outputString	= "0000000000000000";
	unsigned short int			tempValue 		= 16;
	static const char* const	lut 			= "0123456789ABCDEF";
	for (int i = 15;i>=0;--i) {
	tempValue = 0;
	for (unsigned short int k = 0;k<4;++k) {
		if (inputNumber.bit((i+1)*4-k-1) == 1)
			tempValue += static_cast <unsigned short int>(pow(2.0, 3-k));
		}
		outputString[15-i] = lut[tempValue];
	}
	return outputString;
}

string decodeApUint8(ap_uint<8> inputNumber) {
	string 						outputString	= "00";
	unsigned short int			tempValue 		= 16;
	static const char* const	lut 			= "0123456789ABCDEF";
	for (int i = 1;i>=0;--i) {
	tempValue = 0;
	for (unsigned short int k = 0;k<4;++k) {
		if (inputNumber.bit((i+1)*4-k-1) == 1)
			tempValue += static_cast <unsigned short int>(pow(2.0, 3-k));
		}
		outputString[1-i] = lut[tempValue];
	}
	return outputString;
}

ap_uint<64> encodeApUint64(string dataString){
	ap_uint<64> tempOutput = 0;

	for (int i = 0; i < 64/8; i++)
	{
		uint16_t temp;
		std::stringstream parser(dataString.substr(i*2, 2));
		parser >> std::hex >> temp;
		tempOutput(63-(i*8), 56-(i*8)) = temp;
	}

	return tempOutput;
}

ap_uint<8> encodeApUint8(string keepString){
	ap_uint<8> tempOutput = 0;
	unsigned short int	tempValue = 16;
	static const char* const	lut = "0123456789ABCDEF";

	for (unsigned short int i = 0; i<2;++i) {
		for (unsigned short int j = 0;j<16;++j) {
			if (lut[j] == keepString[i]) {
				tempValue = j;
				break;
			}
		}
		if (tempValue != 16) {
			for (short int k = 3;k>=0;--k) {
				if (tempValue >= pow(2.0, k)) {
					tempOutput.bit(7-(4*i+(3-k))) = 1;
					tempValue -= static_cast <unsigned short int>(pow(2.0, k));
				}
			}
		}
	}
	return tempOutput;
}

ap_uint<16> checksumComputation(deque<net_axis<64> >	pseudoHeader) {
	ap_uint<32> tcpChecksum = 0;
	for (size_t i=0;i<pseudoHeader.size();++i) {
		ap_uint<64> tempInput = (pseudoHeader[i].data.range(7, 0), pseudoHeader[i].data.range(15, 8), pseudoHeader[i].data.range(23, 16), pseudoHeader[i].data.range(31, 24), pseudoHeader[i].data.range(39, 32), pseudoHeader[i].data.range(47, 40), pseudoHeader[i].data.range(55, 48), pseudoHeader[i].data.range(63, 56));
		//cerr << hex << tempInput << " " << pseudoHeader[i].data << endl;
		tcpChecksum = ((((tcpChecksum + tempInput.range(63, 48)) + tempInput.range(47, 32)) + tempInput.range(31, 16)) + tempInput.range(15, 0));
		tcpChecksum = (tcpChecksum & 0xFFFF) + (tcpChecksum >> 16);
		tcpChecksum = (tcpChecksum & 0xFFFF) + (tcpChecksum >> 16);
	}
//	tcpChecksum = tcpChecksum.range(15, 0) + tcpChecksum.range(19, 16);
	tcpChecksum = ~tcpChecksum;			// Reverse the bits of the result
	return tcpChecksum.range(15, 0);	// and write it into the output
}


//// This version does not work for TCP segments that are too long... overflow happens
//ap_uint<16> checksumComputation(deque<net_axis<64> >	pseudoHeader) {
//	ap_uint<20> tcpChecksum = 0;
//	for (uint8_t i=0;i<pseudoHeader.size();++i) {
//		ap_uint<64> tempInput = (pseudoHeader[i].data.range(7, 0), pseudoHeader[i].data.range(15, 8), pseudoHeader[i].data.range(23, 16), pseudoHeader[i].data.range(31, 24), pseudoHeader[i].data.range(39, 32), pseudoHeader[i].data.range(47, 40), pseudoHeader[i].data.range(55, 48), pseudoHeader[i].data.range(63, 56));
//		//cerr << hex << tempInput << " " << pseudoHeader[i].data << endl;
//		tcpChecksum = ((((tcpChecksum + tempInput.range(63, 48)) + tempInput.range(47, 32)) + tempInput.range(31, 16)) + tempInput.range(15, 0));
//	}
//	tcpChecksum = tcpChecksum.range(15, 0) + tcpChecksum.range(19, 16);
//	tcpChecksum = ~tcpChecksum;			// Reverse the bits of the result
//	return tcpChecksum.range(15, 0);	// and write it into the output
//}

ap_uint<16> recalculateChecksum(deque<net_axis<64> > inputPacketizer) {
	ap_uint<16> newChecksum = 0;
	// Create the pseudo-header
	ap_uint<16> tcpLength 					= (inputPacketizer[0].data.range(23, 16), inputPacketizer[0].data.range(31, 24)) - 20;
	inputPacketizer[0].data					= (inputPacketizer[2].data.range(31, 0), inputPacketizer[1].data.range(63, 32));
	inputPacketizer[1].data.range(15, 0)	= 0x0600;
	inputPacketizer[1].data.range(31, 16)	= (tcpLength.range(7, 0), tcpLength(15, 8));
	inputPacketizer[4].data.range(47, 32)	= 0x0;
	//ap_uint<32> temp	= (tcpLength, 0x0600);
	inputPacketizer[1].data.range(63, 32) 	= inputPacketizer[2].data.range(63, 32);
	for (size_t i=2;i<inputPacketizer.size() -1;++i)
		inputPacketizer[i]= inputPacketizer[i+1];
	inputPacketizer.pop_back();
	//for (uint8_t i=0;i<inputPacketizer.size();++i)
	//	cerr << hex << inputPacketizer[i].data << " " << inputPacketizer[i].keep << endl;
	//newChecksum = checksumComputation(inputPacketizer); 		// Calculate the checksum
	//return newChecksum;
	return checksumComputation(inputPacketizer);
}

short int injectAckNumber(deque<net_axis<64> > &inputPacketizer, map<fourTuple, ap_uint<32> > &sessionList) {
	fourTuple newTuple = fourTuple(inputPacketizer[1].data.range(63, 32), inputPacketizer[2].data.range(31, 0), inputPacketizer[2].data.range(47, 32), inputPacketizer[2].data.range(63, 48));
	//cerr << hex << inputPacketizer[4].data << endl;
	if (inputPacketizer[4].data.bit(9))	{													// If this packet is a SYN there's no need to inject anything
		//cerr << " Open Tuple: " << hex << inputPacketizer[1].data.range(63, 32) << " - " << inputPacketizer[2].data.range(31, 0) << " - " << inputPacketizer[2].data.range(47, 32) << " - " << inputPacketizer[2].data.range(63, 48) << endl;
		if (sessionList.find(newTuple) != sessionList.end()) {
			cerr << "WARNING: Trying to open an existing session! - " << simCycleCounter << endl;
			return -1;
		}
		else {
			sessionList[newTuple] = 0;
			cerr << "INFO: Opened new session - " << simCycleCounter << endl;
			return 0;
		}
	}
	else {																					// if it is any other packet
		if (sessionList.find(newTuple) != sessionList.end()) {
			inputPacketizer[3].data.range(63, 32) 	= sessionList[newTuple];				// inject the oldest acknowldgement number in the ack number deque
			//cerr << hex << "Input: " << inputPacketizer[3].data.range(63, 32) << endl;
			ap_uint<16> tempChecksum = recalculateChecksum(inputPacketizer);
			//inputPacketizer[4].data.range(47, 32) 	= recalculateChecksum(inputPacketizer);	// and recalculate the checksum
			inputPacketizer[4].data.range(47, 32) = (tempChecksum.range(7, 0), tempChecksum(15, 8));
			//inputPacketizer[4].data.range(47, 32) 	= 0xB13D;
			//for (uint8_t i=0;i<inputPacketizer.size();++i)
			//	cerr << hex << inputPacketizer[i].data << endl;
			return 1;
		}
		else {
			cerr << "WARNING: Trying to send data to a non-existing session!" << endl;
			return -1;
		}
	}
}

bool parseOutputPacket(deque<net_axis<64> > &outputPacketizer, map<fourTuple, ap_uint<32> > &sessionList, deque<net_axis<64> > &inputPacketizer) {		// Looks for an ACK packet in the output stream and when found if stores the ackNumber from that packet into
// the seqNumbers deque and clears the deque containing the output packet.
	bool returnValue = false;
	bool finPacket = false;
	static int pOpacketCounter = 0;
	static ap_uint<32> oldSeqNumber = 0;
	if (outputPacketizer[4].data.bit(9) && !outputPacketizer[4].data.bit(12)) { // Check if this is a SYN packet and if so reply with a SYN-ACK
		inputPacketizer.push_back(outputPacketizer[0]);
		ap_uint<32> ipBuffer = outputPacketizer[1].data.range(63, 32);
		outputPacketizer[1].data.range(63, 32) = outputPacketizer[2].data.range(31, 0);
		inputPacketizer.push_back(outputPacketizer[1]);
		outputPacketizer[2].data.range(31, 0) = ipBuffer;
		ap_uint<16> portBuffer = outputPacketizer[2].data.range(47, 32);
		outputPacketizer[2].data.range(47, 32) = outputPacketizer[2].data.range(63, 48);
		outputPacketizer[2].data.range(63, 48) = portBuffer;
		inputPacketizer.push_back(outputPacketizer[2]);
		ap_uint<32> reversedSeqNumber = (outputPacketizer[3].data.range(7,0), outputPacketizer[3].data.range(15, 8), outputPacketizer[3].data.range(23, 16), outputPacketizer[3].data.range(31, 24)) + 1;
		reversedSeqNumber = (reversedSeqNumber.range(7, 0), reversedSeqNumber.range(15, 8), reversedSeqNumber.range(23, 16), reversedSeqNumber.range(31, 24));
		outputPacketizer[3].data.range(31, 0) = outputPacketizer[3].data.range(63, 32);
		outputPacketizer[3].data.range(63, 32) = reversedSeqNumber;
		inputPacketizer.push_back(outputPacketizer[3]);
		outputPacketizer[4].data.bit(12) = 1;												// Set the ACK bit
		ap_uint<16> tempChecksum = recalculateChecksum(outputPacketizer);
		outputPacketizer[4].data.range(47, 32) = (tempChecksum.range(7, 0), tempChecksum(15, 8));
		//inputPacketizer.push_back(outputPacketizer[4]);
		/*cerr << hex << outputPacketizer[0].data << endl;
		cerr << hex << outputPacketizer[1].data << endl;
		cerr << hex << outputPacketizer[2].data << endl;
		cerr << hex << outputPacketizer[3].data << endl;
		cerr << hex << outputPacketizer[4].data << endl;*/
	}
	else if (outputPacketizer[4].data.bit(8) && !outputPacketizer[4].data.bit(12)) 	// If the FIN bit is set but without the ACK bit being set at the same time
		sessionList.erase(fourTuple(outputPacketizer[1].data.range(63, 32), outputPacketizer[2].data.range(31, 0), outputPacketizer[2].data.range(47, 32), outputPacketizer[2].data.range(63, 48))); // Erase the tuple for this session from the map
	else if (outputPacketizer[4].data.bit(12)) { // If the ACK bit is set
		uint16_t packetLength = reverse((ap_uint<16>) outputPacketizer[0].data.range(31, 16));
		ap_uint<32> reversedSeqNumber = (outputPacketizer[3].data.range(7,0), outputPacketizer[3].data.range(15, 8), outputPacketizer[3].data.range(23, 16), outputPacketizer[3].data.range(31, 24));
		if (outputPacketizer[4].data.bit(9) || outputPacketizer[4].data.bit(8))
			reversedSeqNumber++;
		if (packetLength >= 40) {
			packetLength -= 40;
			reversedSeqNumber += packetLength;
		}
		reversedSeqNumber = (reversedSeqNumber.range(7, 0), reversedSeqNumber.range(15, 8), reversedSeqNumber.range(23, 16), reversedSeqNumber.range(31, 24));
		fourTuple packetTuple = fourTuple(outputPacketizer[2].data.range(31, 0), outputPacketizer[1].data.range(63, 32), outputPacketizer[2].data.range(63, 48), outputPacketizer[2].data.range(47, 32));
		sessionList[packetTuple] = reversedSeqNumber;
		returnValue = true;
		if (outputPacketizer[4].data.bit(8)) {	// This might be a FIN segment at the same time. In this case erase the session from the list
			uint8_t itemsErased = sessionList.erase(fourTuple(outputPacketizer[2].data.range(31, 0), outputPacketizer[1].data.range(63, 32), outputPacketizer[2].data.range(63, 48), outputPacketizer[2].data.range(47, 32))); // Erase the tuple for this session from the map
			finPacket = true;
			//cerr << "Close Tuple: " << hex << outputPacketizer[2].data.range(31, 0) << " - " << outputPacketizer[1].data.range(63, 32) << " - " << inputPacketizer[2].data.range(63, 48) << " - " << outputPacketizer[2].data.range(47, 32) << endl;
			if (itemsErased != 1)
				cerr << "WARNING: Received FIN segment for a non-existing session - " << simCycleCounter << endl;
			else
				cerr << "INFO: Session closed successfully - " << simCycleCounter << endl;
		}
		// Check if the ACK packet also constains data. If it does generate an ACK for. Look into the IP header length for this.
		if (packetLength > 0 || finPacket == true) { // 20B of IP Header & 20B of TCP Header since we never generate options
			finPacket = false;
			outputPacketizer[0].data.range(31, 16) = 0x2800;
			inputPacketizer.push_back(outputPacketizer[0]);
			ap_uint<32> ipBuffer = outputPacketizer[1].data.range(63, 32);
			outputPacketizer[1].data.range(63, 32) = outputPacketizer[2].data.range(31, 0);
			inputPacketizer.push_back(outputPacketizer[1]);
			outputPacketizer[2].data.range(31, 0) = ipBuffer;
			ap_uint<16> portBuffer = outputPacketizer[2].data.range(47, 32);
			outputPacketizer[2].data.range(47, 32) = outputPacketizer[2].data.range(63, 48);
			outputPacketizer[2].data.range(63, 48) = portBuffer;
			inputPacketizer.push_back(outputPacketizer[2]);
			//ap_uint<32> seqNumber = outputPacketizer[3].data.range(31, 0);
			outputPacketizer[3].data.range(31, 0) = outputPacketizer[3].data.range(63, 32);
			outputPacketizer[3].data.range(63, 32) = reversedSeqNumber;
			//cerr << hex << outputPacketizer[3].data.range(63, 32) << " - " << reversedSeqNumber << endl;
			inputPacketizer.push_back(outputPacketizer[3]);
			outputPacketizer[4].data.bit(12) = 1;												// Set the ACK bit
			outputPacketizer[4].data.bit(8) = 0;												// Unset the FIN bit
			ap_uint<16> tempChecksum = recalculateChecksum(outputPacketizer);
			outputPacketizer[4].data.range(47, 32) = (tempChecksum.range(7, 0), tempChecksum(15, 8));
			outputPacketizer[4].keep = 0x3F;
			outputPacketizer[4].last = 1;
			inputPacketizer.push_back(outputPacketizer[4]);
			/*cerr << std::hex << outputPacketizer[0].data << " - " << outputPacketizer[0].keep << " - " << outputPacketizer[0].last << endl;
			cerr << std::hex << outputPacketizer[1].data << " - " << outputPacketizer[1].keep << " - " << outputPacketizer[1].last << endl;
			cerr << std::hex << outputPacketizer[2].data << " - " << outputPacketizer[2].keep << " - " << outputPacketizer[2].last << endl;
			cerr << std::hex << outputPacketizer[3].data << " - " << outputPacketizer[3].keep << " - " << outputPacketizer[3].last << endl;
			cerr << std::hex << outputPacketizer[4].data << " - " << outputPacketizer[4].keep << " - " << outputPacketizer[4].last << endl;*/
			if (oldSeqNumber != reversedSeqNumber) {
				pOpacketCounter++;
				//cerr << "ACK cnt: " << dec << pOpacketCounter << hex << " - " << outputPacketizer[3].data.range(63, 32) << endl;
			}
			oldSeqNumber = reversedSeqNumber;
		}
		//cerr << hex << "Output: " << outputPacketizer[3].data.range(31, 0) << endl;
	}
	outputPacketizer.clear();
	return returnValue;
}

void flushInputPacketizer(deque<net_axis<64> > &inputPacketizer, stream<net_axis<64> > &ipRxData, map<fourTuple, ap_uint<32> > &sessionList) {
	if (inputPacketizer.size() != 0) {
		injectAckNumber(inputPacketizer, sessionList);
		size_t inputPacketizerSize = inputPacketizer.size();
		for (size_t i=0;i<inputPacketizerSize;++i) {
			net_axis<64> temp = inputPacketizer.front();
			ipRxData.write(temp);
			inputPacketizer.pop_front();
		}
	}
}

vector<string> parseLine(string stringBuffer) {
	vector<string> tempBuffer;
	bool found = false;

	while (stringBuffer.find(" ") != string::npos)	// Search for spaces delimiting the different data words
	{
		string temp = stringBuffer.substr(0, stringBuffer.find(" "));							// Split the the string
		stringBuffer = stringBuffer.substr(stringBuffer.find(" ")+1, stringBuffer.length());	// into two
		tempBuffer.push_back(temp);		// and store the new part into the vector. Continue searching until no more spaces are found.
	}
	tempBuffer.push_back(stringBuffer);	// and finally push the final part of the string into the vector when no more spaces are present.
	return tempBuffer;
}

template <int W, int X>
void convertPacketWidth(hls::stream<net_axis<W> >&	input, hls::stream<net_axis<X> >& output)
{
	while(!input.empty())
	{
		convertStreamWidth<W, 24>(input, output);
	}
}

int main(int argc, char *argv[]) {
	stream<net_axis<DATA_WIDTH> >						ipRxData("ipRxData");
	stream<mmStatus>					rxBufferWriteStatus("rxBufferWriteStatus");
	stream<mmStatus>					txBufferWriteStatus("txBufferWriteStatus");
	stream<net_axis<DATA_WIDTH> >						rxBufferReadData("rxBufferReadData");
	stream<net_axis<DATA_WIDTH> >						txBufferReadData("txBufferReadData");
	stream<net_axis<DATA_WIDTH> >						ipTxData("ipTxData");
	stream<mmCmd>						rxBufferWriteCmd("rxBufferWriteCmd");
	stream<mmCmd>						rxBufferReadCmd("rxBufferReadCmd");
	stream<mmCmd>						txBufferWriteCmd("txBufferWriteCmd");
	stream<mmCmd>						txBufferReadCmd("txBufferReadCmd");
	stream<net_axis<DATA_WIDTH> >						rxBufferWriteData("rxBufferWriteData");
	stream<net_axis<DATA_WIDTH> >						txBufferWriteData("txBufferWriteData");
	stream<rtlSessionLookupReply>		sessionLookup_rsp("sessionLookup_rsp");
	stream<rtlSessionUpdateReply>		sessionUpdate_rsp("sessionUpdate_rsp");
	stream<rtlSessionLookupRequest>		sessionLookup_req("sessionLookup_req");
	stream<rtlSessionUpdateRequest>		sessionUpdate_req("sessionUpdate_req");
//	stream<rtlSessionUpdateRequest>		sessionUpdate_req;
	stream<ap_uint<16> >				listenPortReq("listenPortReq");
	stream<appReadRequest>				rxDataReq("rxDataReq");
	stream<ipTuple>						openConnReq("openConnReq");
	stream<ap_uint<16> >				closeConnReq("closeConnReq");
	stream<appTxMeta>					txDataReqMeta("txDataReqMeta");
	stream<net_axis<DATA_WIDTH> >						txDataReq("txDataReq");
	stream<bool>						listenPortRsp("listenPortRsp");
	stream<appNotification>				notification("notification");
	stream<ap_uint<16> >				rxDataRspMeta("rxDataRspMeta");
	stream<net_axis<DATA_WIDTH> >						rxDataRsp("rxDataRsp");
	stream<openStatus>					openConnRsp("openConnRsp");
	stream<appTxRsp>					txDataRsp("txDataRsp");
	ap_uint<16>							regSessionCount;
	//ap_uint<16>							relSessionCount;
	stream<net_axis<64> >						ipRxData64;
	stream<net_axis<64> >						rxDataRsp64;
	stream<net_axis<64> >						ipTxData64;
	stream<net_axis<64> >						txDataReq64;
	net_axis<64>								ipTxDataOut_Data;
	net_axis<64>								ipRxData_Data;
	net_axis<64>								ipTxDataIn_Data;
	stream<net_axis<64> > 					rxDataOut("rxDataOut");						// This stream contains the data output from the Rx App I/F
	net_axis<64>								rxDataOut_Data;								// This variable is where the data read from the stream above is temporarily stored before output

	dummyMemory<DATA_WIDTH> rxMemory;
	dummyMemory<DATA_WIDTH> txMemory;
	
	map<fourTuple, ap_uint<32> > sessionList;

	deque<net_axis<64> >	inputPacketizer;
	deque<net_axis<64> >	outputPacketizer;						// This deque collects the output data word until a whole packet is accumulated.

	ifstream	rxInputFile;
	ifstream	txInputFile;
	ofstream	rxOutput;
	ofstream	txOutput;
	ifstream	rxGold;
	ifstream	txGold;
	unsigned int	skipCounter 		= 0;
	cycleCounter = 0;
	simCycleCounter		= 0;
	unsigned int	myCounter		= 0;
	bool		idleCycle		= 0;
	string		dataString, keepString;
	vector<string> 	stringVector;
	vector<string>	txStringVector;
	bool 		firstWordFlag		= true;
	int 		rxGoldCompare		= 0;
	int			txGoldCompare		= 0; // not used yet
	int			returnValue		= 0;
	uint16_t	txWordCounter 		= 0;
	uint16_t	rxPayloadCounter	= 0; 	// This variable counts the number of packets output during the simulation on the Rx side
	uint16_t	txPacketCounter		= 0;	// This variable countrs the number of packet send from the Tx side (total number of packets, all kinds, from all sessions).
	bool		testRxPath		= true;	// This variable indicates if the Rx path is to be tested, thus it remains true in case of Rx only or bidirectional testing
	bool		testTxPath		= true;	// Same but for the Tx path.
	vector<ap_uint<16> > txSessionIDs;		// This vector holds the session IDs of the session to which data are transmitted.
	uint16_t	currentTxSessionID	= 0;	// Holds the session ID which was last used to send data into the Tx path.
	char 		mode 		= *argv[1];

	if (argc > 7 || argc < 4) {
		cout << "Expected format: mode input_file rx_output_file tx_output_file (rx_gold_output)" << endl;
//		cout << "You need to provide three parameters (the input file name followed by the output file name for the Rx Data and the one for the Tx Data)!" << endl;
		return -1;
	}
	if (mode == '0')  { // Test the Rx side only
		rxInputFile.open(argv[2]);
		if (!rxInputFile) {
			cout << " Error opening Rx Input file!" << endl;
			return -1;
		}
		rxOutput.open(argv[3]);
		if (!rxOutput) {
			cout << " Error opening Rx Output file!" << endl;
			return -1;
		}
		txOutput.open(argv[4]);
		if (!txOutput) {
			cout << " Error opening Tx Output file!" << endl;
			return -1;
		}
		if(argc == 6) {
			rxGold.open(argv[5]);
			if (!rxGold) {
				cout << " Error accessing Gold Rx file!" << endl;
				return -1;
			}
		}
		testTxPath = false;
	}
	else if (mode == '1') { // Test the Tx side only
		txInputFile.open(argv[2]);
		if (!txInputFile) {
			cout << " Error opening Tx Input file!" << endl;
			return -1;
		}
		txOutput.open(argv[3]);
		if (!txOutput) {
			cout << " Error opening Tx Output file!" << endl;
			return -1;
		}
		if(argc == 5){
			txGold.open(argv[4]);
			if (!txGold) {
				cout << " Error accessing Gold Tx file!" << endl;
				return -1;
			}
		}
		testRxPath = false;
	}
	else if (mode == '2') { // Test the Rx & Tx side together
		rxInputFile.open(argv[2]);
		if (!rxInputFile) {
			cout << " Error opening Rx Input file!" << endl;
			return -1;
		}
		txInputFile.open(argv[3]);
		if (!txInputFile) {
			cout << " Error opening Tx Input file!" << endl;
			return -1;
		}
		rxOutput.open(argv[4]);
		if (!rxOutput) {
			cout << " Error opening Rx Output file!" << endl;
			return -1;
		}
		txOutput.open(argv[5]);
		if (!txOutput) {
			cout << " Error opening Tx Output file!" << endl;
			return -1;
		}
		if(argc == 7){
			rxGold.open(argv[6]);
			if (!rxGold) {
				cout << " Error accessing Gold Rx file!" << endl;
				return -1;
			}
		}
		else if (argc == 6 ) {
			cout << "Error! Bi-directional testing requires two golden output files to be passed!" << endl;
			return -1;
		}
	}
	else { // Other cases are not allowed, exit.
		cout << "First argument can be: 0 - Rx path testing only, 1 - Tx path testing only, 2 - Bi-directional testing." << endl;
		return -1;
	}
	if (testTxPath == true) { // If the Tx Path will be tested then open a session for testing.
		for (uint8_t i=0;i<noOfTxSessions;++i) { 
			ipTuple newTuple = {150*(i+65355), 10*(i+65355)}; 		// IP address and port to open
			openConnReq.write(newTuple); 	// Write into TOE Tx I/F queue
		}
	}
//	while (simCycleCounter < totalSimCycles) {
	static int loopCount = 0;
	do {
		loopCount++;
		if (loopCount % 1000 == 0) {
			// cerr << "[TB] Loop iteration: " << loopCount << endl;
		}
		if (idleCycle == true) {
			if (cycleCounter >= skipCounter) {
				cycleCounter = 0;
				idleCycle = false;
			}
			else
				cycleCounter++;
		}
		else if (idleCycle == false) {
			unsigned short int temp;
			string stringBuffer;
			string txStringBuffer;
			flushInputPacketizer(inputPacketizer, ipRxData64, sessionList);			// Before processing the input file data words, write any packets generated from the B itself
			convertPacketWidth(ipRxData64, ipRxData);
			if (testRxPath == true) {
				//flushInputPacketizer(inputPacketizer, ipRxData, sessionList);			// Before processing the input file data words, write any packets generated from the B itself
				getline(rxInputFile, stringBuffer);
				stringVector = parseLine(stringBuffer);
				if (stringVector[0] == "W") {	// Take into account wait cycles in the Rx input files. If they coincide with wait cycles in the Tx input files (only in case of idirectional testing, then the Tx side ones takes precedence.)
					skipCounter = atoi(stringVector[1].c_str());
					idleCycle = true;
				}
				else if(rxInputFile.fail() == 1 || stringBuffer.empty()){
				//break;
				}
				else {
					static uint16_t inputPacketCount = 0;
					do {
						if (firstWordFlag == false) {
							getline(rxInputFile, stringBuffer);
							if (rxInputFile.fail() || stringBuffer.empty()) {
								cerr << "ERROR: Unexpected EOF or empty line in input file!" << endl;
								break;
							}
							stringVector = parseLine(stringBuffer);
							if (stringVector.size() < 3) {
								cerr << "ERROR: Invalid line format: " << stringBuffer << endl;
								break;
							}
						}
						firstWordFlag = false;
						string tempString = "0000000000000000";
						ipRxData_Data = net_axis<64>(encodeApUint64(stringVector[0]), encodeApUint8(stringVector[2]), atoi(stringVector[1].c_str()));
						inputPacketizer.push_back(ipRxData_Data);
						
						// std::cout << "Input Packetizer Size: " << inputPacketizer.size() << std::endl;
					} while (ipRxData_Data.last != 1);
					firstWordFlag = true;

					// === Extract TCP Payload from input packet ===
					// Packet structure: Word 0-4 = IP+TCP headers (40 bytes), Word 5+ = Payload
					if (inputPacketizer.size() > 5) {  // Has payload
						ExpectedPayload payload;
						payload.packetIndex = inputPacketCount;

						// Extract payload bytes (starting from word 5)
						for (size_t w = 5; w < inputPacketizer.size(); w++) {
							net_axis<64>& word = inputPacketizer[w];
							for (int b = 0; b < 8; b++) {
								if (word.keep[b]) {
									payload.data.push_back((uint8_t)(word.data((b*8)+7, b*8)));
								}
							}
						}

						if (payload.data.size() > 0) {
							expectedPayloads.push_back(payload);
							cerr << "[Input Packet " << inputPacketCount << "] TCP Payload: "
							     << payload.data.size() << " bytes - ";
							for (size_t i = 0; i < min((size_t)16, payload.data.size()); i++) {
								cerr << hex << setw(2) << setfill('0') << (int)payload.data[i] << " ";
							}
							if (payload.data.size() > 16) cerr << "...";
							cerr << endl;
						}
					}
					inputPacketCount++;
					// === End Extract ===

					// cerr << "[TB] Before flushInputPacketizer, inputPacketizer.size=" << inputPacketizer.size() << endl;
					flushInputPacketizer(inputPacketizer, ipRxData64, sessionList);
					// cerr << "[TB] After flushInputPacketizer, ipRxData64.size=" << ipRxData64.size() << endl;
					// cerr << "[TB] Before convertPacketWidth" << endl;
					convertPacketWidth(ipRxData64, ipRxData);
					// cerr << "[TB] After convertPacketWidth, ipRxData.size=" << ipRxData.size() << endl;
				}
			}
			if (testTxPath == true && txSessionIDs.size() > 0) {
				//flushInputPacketizer(inputPacketizer, ipRxData, sessionList);			// Before processing the input file data words, write any packets generated from the B itself
				getline(txInputFile, txStringBuffer);
				txStringVector = parseLine(txStringBuffer);
				if (txStringVector[0] == "W") {	// Only the Rx Path testing can 
					skipCounter = atoi(txStringVector[1].c_str());
					idleCycle = true;
				}
				else if(txInputFile.fail() || txStringBuffer.empty()){
					//break;
					//return 0;
				}
				else { // Send data only after a session has been opened on the Tx Side
					do {
						if (firstWordFlag == false) { // If this isn't the first word of the packet then data have to be read in from the file
							getline(txInputFile, txStringBuffer);
							txStringVector = parseLine(txStringBuffer);
						}
						else { // If this is the first word of the data then the request has to be written into the Tx App I/F
							txDataReqMeta.write(appTxMeta(txSessionIDs[currentTxSessionID], 1460)); //TODO fix this
							currentTxSessionID == noOfTxSessions - 1 ? currentTxSessionID = 0 : currentTxSessionID++;
						}
						firstWordFlag = false;
						string tempString = "0000000000000000";
						ipTxDataIn_Data = net_axis<64>(encodeApUint64(txStringVector[0]), encodeApUint8(txStringVector[2]), atoi(txStringVector[1].c_str()));
						txDataReq64.write(ipTxDataIn_Data);
						// std::cout << "Tx Input Packetizer Size: " << txDataReq64.size() << std::endl;
					} while (ipTxDataIn_Data.last != 1);
					convertPacketWidth(txDataReq64, txDataReq);
					firstWordFlag = true;
				}
			}
//			}
		}
		// cerr << "[TB] Before toe() - ipRxData.size=" << ipRxData.size() << endl;
#if !(RX_DDR_BYPASS)
		toe(ipRxData, rxBufferWriteStatus, txBufferWriteStatus, rxBufferReadData, txBufferReadData, ipTxData, rxBufferWriteCmd,
			rxBufferReadCmd, txBufferWriteCmd, txBufferReadCmd, rxBufferWriteData, txBufferWriteData, sessionLookup_rsp, sessionUpdate_rsp,
			sessionLookup_req, sessionUpdate_req, listenPortReq, rxDataReq, openConnReq, closeConnReq, txDataReqMeta, txDataReq,
			listenPortRsp, notification, rxDataRspMeta, rxDataRsp, openConnRsp, txDataRsp, 0x01010101, regSessionCount);
#else
		toe(ipRxData, txBufferWriteStatus, rxBufferReadData, txBufferReadData, ipTxData,
			txBufferWriteCmd, txBufferReadCmd, rxBufferWriteData, txBufferWriteData, sessionLookup_rsp, sessionUpdate_rsp,
			sessionLookup_req, sessionUpdate_req, listenPortReq, rxDataReq, openConnReq, closeConnReq, txDataReqMeta, txDataReq,
			listenPortRsp, notification, rxDataRspMeta, rxDataRsp, openConnRsp, txDataRsp, 0, 0, 0x01010101, regSessionCount);
#endif
		// cerr << "[TB] After toe()" << endl;

		//TODO use actual iperf
		convertStreamWidth<DATA_WIDTH, 22>(rxDataRsp, rxDataRsp64);
		iperf(listenPortReq, listenPortRsp, notification, rxDataReq,
			  rxDataRspMeta, rxDataRsp64, rxDataOut, openConnReq, openConnRsp,
			  closeConnReq, txSessionIDs);
		// cerr << "[TB] After iperf()" << endl;

		simulateRx<DATA_WIDTH>(&rxMemory, rxBufferWriteCmd, rxBufferWriteStatus, rxBufferReadCmd, rxBufferWriteData, rxBufferReadData);
		simulateTx<DATA_WIDTH>(&txMemory, txBufferWriteCmd, txBufferWriteStatus, txBufferReadCmd, txBufferWriteData, txBufferReadData);
		  		   sessionLookupStub(sessionLookup_req, sessionLookup_rsp,	sessionUpdate_req, sessionUpdate_rsp);

		convertStreamWidth<DATA_WIDTH,23>(ipTxData, ipTxData64);
		if (!ipTxData64.empty()) {
			ipTxData64.read(ipTxDataOut_Data);
			string dataOutput = decodeApUint64(ipTxDataOut_Data.data);
			string keepOutput = decodeApUint8(ipTxDataOut_Data.keep);
			txOutput << dataOutput << " " << ipTxDataOut_Data.last << " " << keepOutput << endl;
			outputPacketizer.push_back(ipTxDataOut_Data);
			if (ipTxDataOut_Data.last == 1) {				// If the whole packet has been written into the deque
				parseOutputPacket(outputPacketizer, sessionList, inputPacketizer);	// then parse it.
				txWordCounter = 0;
				txPacketCounter++;
				cerr << dec << txPacketCounter << ", " << endl << hex;
			}
			else
				txWordCounter++;
		}
		if (!rxDataOut.empty()) {
			rxDataOut.read(rxDataOut_Data);
			string dataOutput = decodeApUint64(rxDataOut_Data.data);
			string keepOutput = decodeApUint8(rxDataOut_Data.keep);
//			cout << rxDataOut_Data.keep << endl;
			for (unsigned short int i = 0; i<8; ++i) { // delete the data not to be kept by "keep" - for Golden comparison
				if(rxDataOut_Data.keep[7-i] == 0){
//					cout << "rxDataOut_Data.keep[" << i << "] = " << rxDataOut_Data.keep[i] << endl;
//					cout << "dataOutput " << dataOutput[i*2] << dataOutput[(i*2)+1] << " is replaced by 00" << endl;
					dataOutput.replace(i*2,2,"00");
				}
			}

			// Collect received data for End-to-End verification
			for (int i = 0; i < 8; i++) {
				if (rxDataOut_Data.keep[i]) {
					currentReceivedPayload.data.push_back((uint8_t)(rxDataOut_Data.data((i*8)+7, i*8)));
				}
			}

			if (rxDataOut_Data.last == 1) {
				rxPayloadCounter++;
				// Save completed payload
				if (currentReceivedPayload.data.size() > 0) {
					receivedPayloads.push_back(currentReceivedPayload);
					cerr << "[rxDataOut] Received payload " << receivedPayloads.size() - 1
					     << ": " << currentReceivedPayload.data.size() << " bytes" << endl;
				}
				currentReceivedPayload.data.clear();
			}
			rxOutput << dataOutput << " " << rxDataOut_Data.last << " " << keepOutput << endl;
		}
		if (!txDataRsp.empty()) {
			appTxRsp tempResp = txDataRsp.read();
			if (tempResp.error != 0)
				cerr << endl << "Warning: Attempt to write data into the Tx App I/F of the TOE was unsuccesfull. Returned error code: " << tempResp.error << endl;
		}
		simCycleCounter++;
		// cout << dec << simCycleCounter << endl;
		//cerr << simCycleCounter << " - Number of Sessions opened: " <<  dec << regSessionCount << endl << "Number of Sessions closed: " << relSessionCount << endl;
	} while (simCycleCounter < totalSimCycles);
//	while (!ipTxData.empty() || !ipRxData.empty() || !rxDataOut.empty());
	/*while (!txBufferWriteCmd.empty()) {
		mmCmd tempMemCmd = txBufferWriteCmd.read();
		std::cerr <<  "Left-over Cmd: " << std::hex << tempMemCmd.saddr << " - " << tempMemCmd.bbt << std::endl;
	}*/

	// === DDR Memory Verification ===
	// Compare INPUT FILE TCP payloads with what's stored in dummyMemory
	std::cerr << "\n=== DDR Memory Verification (Input File vs dummyMemory) ===" << std::endl;
	std::cerr << "Expected payloads from input file: " << expectedPayloads.size() << std::endl;
	std::cerr << "DDR write records: " << ddrWriteRecords.size() << std::endl;

	int passCount = 0;
	int failCount = 0;

	// Match expected payloads with DDR write records
	// Handle wrap-around splits: one input packet may become two DDR writes
	size_t i = 0;  // index for expectedPayloads
	size_t j = 0;  // index for ddrWriteRecords

	while (i < expectedPayloads.size() && j < ddrWriteRecords.size()) {
		ExpectedPayload& expected = expectedPayloads[i];
		DdrWriteRecord& rec = ddrWriteRecords[j];

		// Check for wrap-around split: 3 conditions must be met
		bool isWrapSplit = false;
		if (j + 1 < ddrWriteRecords.size()) {
			DdrWriteRecord& nextRec = ddrWriteRecords[j + 1];
			isWrapSplit =
				(rec.addr + rec.length == BUFFER_SIZE) &&           // First ends exactly at buffer end
				(nextRec.addr == 0) &&                               // Second starts exactly at 0
				(rec.length + nextRec.length == expected.data.size()); // Combined length matches expected
		}

		std::cerr << "\n--- Verification " << i << " (Packet " << expected.packetIndex << ") ---" << std::endl;

		if (isWrapSplit) {
			DdrWriteRecord& nextRec = ddrWriteRecords[j + 1];
			std::cerr << "[WRAP-AROUND SPLIT DETECTED]" << std::endl;
			std::cerr << "Part 1: SessionID=0x" << std::hex << (uint16_t)rec.sessionId
			          << ", Addr=0x" << (uint32_t)rec.addr
			          << ", Len=" << std::dec << rec.length << std::endl;
			std::cerr << "Part 2: SessionID=0x" << std::hex << (uint16_t)nextRec.sessionId
			          << ", Addr=0x" << (uint32_t)nextRec.addr
			          << ", Len=" << std::dec << nextRec.length << std::endl;
			std::cerr << "Combined Length: " << (rec.length + nextRec.length) << " bytes" << std::endl;
			std::cerr << "Input File Payload Length: " << expected.data.size() << " bytes" << std::endl;

			// Verify first part
			bool match1 = rxMemory.verifyData(rec.sessionId, rec.addr,
			                                   expected.data.data(), rec.length);
			// Verify second part
			bool match2 = rxMemory.verifyData(nextRec.sessionId, nextRec.addr,
			                                   expected.data.data() + rec.length, nextRec.length);

			if (match1 && match2) {
				std::cerr << "Result: PASS - Wrap-around split data matches" << std::endl;
				passCount++;
			} else {
				std::cerr << "Result: FAIL - Wrap-around split data mismatch (part1="
				          << match1 << ", part2=" << match2 << ")" << std::endl;
				failCount++;
			}

			j += 2;  // Consume both DDR records
		} else {
			// Normal case: one-to-one matching
			std::cerr << "SessionID: 0x" << std::hex << (uint16_t)rec.sessionId
			          << ", DDR Addr: 0x" << (uint32_t)rec.addr << std::dec << std::endl;
			std::cerr << "Input File Payload Length: " << expected.data.size() << " bytes" << std::endl;
			std::cerr << "DDR Write Length: " << rec.length << " bytes" << std::endl;

			// Print expected data from input file
			std::cerr << "Input File Data: ";
			for (size_t k = 0; k < expected.data.size() && k < 32; k++) {
				std::cerr << std::hex << std::setw(2) << std::setfill('0') << (int)expected.data[k] << " ";
			}
			if (expected.data.size() > 32) std::cerr << "...";
			std::cerr << std::endl;

			// Print what's in dummyMemory at that address
			std::cerr << "dummyMemory Data: ";
			rxMemory.dumpBuffer(rec.sessionId, rec.addr, std::min((size_t)32, expected.data.size()));

			// Verify: Compare INPUT FILE data with dummyMemory
			bool match = true;
			size_t compareLen = std::min(expected.data.size(), (size_t)rec.length);

			if (expected.data.size() != (size_t)rec.length) {
				std::cerr << "WARNING: Length mismatch! Input=" << std::hex << expected.data.size()
				          << ", DDR=" << rec.length << std::dec << std::endl;
			}

			// Do byte-by-byte comparison
			if (compareLen > 0) {
				match = rxMemory.verifyData(rec.sessionId, rec.addr, expected.data.data(), compareLen);
			}

			if (match && expected.data.size() == (size_t)rec.length) {
				std::cerr << "Result: PASS - Input file TCP payload matches dummyMemory" << std::endl;
				passCount++;
			} else {
				std::cerr << "Result: FAIL - Data mismatch between input file and dummyMemory" << std::endl;
				failCount++;
			}

			j += 1;  // Consume one DDR record
		}

		i++;  // Move to next expected payload
	}

	// Report any unmatched records
	if (i < expectedPayloads.size()) {
		std::cerr << "\nWARNING: " << (expectedPayloads.size() - i)
		          << " input packets had no matching DDR write!" << std::endl;
		failCount += (expectedPayloads.size() - i);
	}
	if (j < ddrWriteRecords.size()) {
		std::cerr << "\nINFO: " << (ddrWriteRecords.size() - j)
		          << " extra DDR writes (possibly retransmits or ACKs)" << std::endl;
	}

	std::cerr << "\n=== DDR Verification Summary ===" << std::endl;
	std::cerr << "Verified: " << i << ", Pass: " << passCount << ", Fail: " << failCount << std::endl;
	if (failCount == 0 && passCount > 0) {
		std::cerr << "SUCCESS: All input file TCP payloads correctly written to dummyMemory!" << std::endl;
	}
	std::cerr << "=== End DDR Verification ===\n" << std::endl;

	// === End-to-End Verification: Input File vs rxDataOut ===
	std::cerr << "\n=== End-to-End Verification (Input File vs rxDataOut) ===" << std::endl;
	std::cerr << "Expected payloads from input file: " << expectedPayloads.size() << std::endl;
	std::cerr << "Received payloads from rxDataOut: " << receivedPayloads.size() << std::endl;

	int e2ePassCount = 0;
	int e2eFailCount = 0;

	size_t e2eMinRecords = std::min(expectedPayloads.size(), receivedPayloads.size());

	for (size_t i = 0; i < e2eMinRecords; i++) {
		ExpectedPayload& expected = expectedPayloads[i];
		ReceivedPayload& received = receivedPayloads[i];

		std::cerr << "\n--- E2E Verification " << i << " (Packet " << expected.packetIndex << ") ---" << std::endl;
		std::cerr << "Input File Payload: " << expected.data.size() << " bytes" << std::endl;
		std::cerr << "rxDataOut Payload: " << received.data.size() << " bytes" << std::endl;

		// Print expected data from input file
		std::cerr << "Input File Data:  ";
		for (size_t j = 0; j < expected.data.size() && j < 32; j++) {
			std::cerr << std::hex << std::setw(2) << std::setfill('0') << (int)expected.data[j] << " ";
		}
		if (expected.data.size() > 32) std::cerr << "...";
		std::cerr << std::endl;

		// Print received data from rxDataOut
		std::cerr << "rxDataOut Data:   ";
		for (size_t j = 0; j < received.data.size() && j < 32; j++) {
			std::cerr << std::hex << std::setw(2) << std::setfill('0') << (int)received.data[j] << " ";
		}
		if (received.data.size() > 32) std::cerr << "...";
		std::cerr << std::endl;

		// Compare
		bool match = true;
		if (expected.data.size() != received.data.size()) {
			std::cerr << "MISMATCH: Length differs! Expected=" << std::dec << expected.data.size()
			          << ", Received=" << received.data.size() << std::endl;
			match = false;
		} else {
			for (size_t j = 0; j < expected.data.size(); j++) {
				if (expected.data[j] != received.data[j]) {
					std::cerr << "MISMATCH at byte " << std::dec << j
					          << ": Expected=0x" << std::hex << (int)expected.data[j]
					          << ", Received=0x" << (int)received.data[j] << std::endl;
					match = false;
					break;
				}
			}
		}

		if (match) {
			std::cerr << "Result: PASS - Input file payload == rxDataOut" << std::dec << std::endl;
			e2ePassCount++;
		} else {
			std::cerr << "Result: FAIL - Data mismatch!" << std::dec << std::endl;
			e2eFailCount++;
		}
	}

	// Report unmatched
	if (expectedPayloads.size() > receivedPayloads.size()) {
		std::cerr << "\nWARNING: " << (expectedPayloads.size() - receivedPayloads.size())
		          << " input payloads were NOT received in rxDataOut!" << std::endl;
		e2eFailCount += (expectedPayloads.size() - receivedPayloads.size());
	}
	if (receivedPayloads.size() > expectedPayloads.size()) {
		std::cerr << "\nINFO: " << (receivedPayloads.size() - expectedPayloads.size())
		          << " extra payloads received (possibly from ACKs/retransmits)" << std::endl;
	}

	std::cerr << "\n=== End-to-End Verification Summary ===" << std::endl;
	std::cerr << "Verified: " << e2eMinRecords << ", Pass: " << e2ePassCount << ", Fail: " << e2eFailCount << std::endl;
	if (e2eFailCount == 0 && e2ePassCount > 0) {
		std::cerr << "SUCCESS: All input file TCP payloads correctly received at application!" << std::endl;
	} else if (e2eFailCount > 0) {
		std::cerr << "FAILURE: Data corruption detected in DDR path!" << std::endl;
	} else if (e2ePassCount == 0 && expectedPayloads.size() > 0) {
		std::cerr << "FAILURE: No data received at application (rxDataOut empty)!" << std::endl;
	}
	std::cerr << "=== End End-to-End Verification ===\n" << std::endl;

	// Compare the results file with the golden results
	// only RX Gold supported for now
	float rxDividedPacketCounter = static_cast <float>(rxPayloadCounter) / 2;
	unsigned int roundedRxPayloadCounter = rxPayloadCounter / 2;
	if (rxDividedPacketCounter > roundedRxPayloadCounter)
		roundedRxPayloadCounter++;
	if (roundedRxPayloadCounter != (txPacketCounter - 1))
		cout << "WARNING: Number of received packets (" << rxPayloadCounter << ") is not equal to the number of Tx Packets (" << txPacketCounter << ")!" << endl;
	// Output Number of Sessions
	//cerr << "Number of Sessions opened: " <<  dec << regSessionCount << endl << "Number of Sessions closed: " << relSessionCount << endl;
	// Convert command line arguments to strings
	if(argc == 5) {
		vector<string> args(argc);
		for (int i=1; i<argc; ++i)
			args[i] = argv[i];

		rxGoldCompare = system(("diff --brief -w "+args[2]+" " + args[4]+" ").c_str());
		//	txGoldCompare = system(("diff --brief -w "+args[3]+" " + args[5]+" ").c_str()); // uncomment when TX Golden comparison is supported

		if (rxGoldCompare != 0){
			cout << "RX Output != Golden RX Output. Simulation FAILED." << endl;
			returnValue = 1;
		}
		else cout << "RX Output == Golden RX Output" << endl;

		//	if (txGoldCompare != 0){
		//		cout << "TX Output != Golden TX Output" << endl;
		//		returnValue = 1;
		//	}
		//	else cout << "TX Output == Golden TX Output" << endl;

		if(rxGoldCompare == 0 && txGoldCompare == 0){
			cout << "Test Passed! (Both Golden files match)" << endl;
			returnValue = 0; // Return 0 if the test passes
		}
	}
	
	if (mode == '0') { // Rx side testing only
		rxInputFile.close();
		rxOutput.close();
		txOutput.close();
		if(argc == 6)
			rxGold.close();
	}
	else if (mode == '1') { // Tx side testing only
		txInputFile.close();
		txOutput.close();
		if(argc == 5)
			txGold.close();
	}
	else if (mode == '2') { // Bi-directional testing
		rxInputFile.close();
		txInputFile.close();
		rxOutput.close();
		txOutput.close();
		if(argc == 7){
			rxGold.close();
			txGold.close();
		}
	}
	return returnValue;
}
