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
#ifndef MEM_H_
#define MEM_H_

#include "toe.hpp"
#include <map>
#include <vector>
#include <iomanip>

template <int WIDTH>
class dummyMemory {
public:
	void setReadCmd(mmCmd cmd);
	void setWriteCmd(mmCmd cmd);
	void readWord(net_axis<WIDTH>& word);
	void writeWord(net_axis<WIDTH>& word);
	// Verification methods
	void dumpBuffer(ap_uint<16> sessionId, uint32_t startAddr, uint32_t length);
	bool verifyData(ap_uint<16> sessionId, uint32_t startAddr, const uint8_t* expectedData, uint32_t length);
	std::vector<ap_uint<16>> getSessionIds();
private:
	std::map<ap_uint<16>, ap_uint<8>*>::iterator createBuffer(ap_uint<16> id);
	//void shuffleWord(ap_uint<64>& );
	//bool* getBitMask(ap_uint<4> keep);
	ap_uint<WINDOW_BITS> readAddr; // Extended for WINDOW_SCALE support
	ap_uint<16> readId;
	int readLen;
	ap_uint<WINDOW_BITS> writeAddr; // Extended for WINDOW_SCALE support
	ap_uint<16> writeId;
	ap_uint<WINDOW_BITS> writeLen;
	// Debug tracking for write commands
	ap_uint<WINDOW_BITS> writeCmdStartAddr;
	ap_uint<WINDOW_BITS> writeCmdLen;
	ap_uint<WINDOW_BITS> writeCmdBytesWritten;
	std::map<ap_uint<16>, ap_uint<8>*> storage;
	std::map<ap_uint<16>, ap_uint<8>*>::iterator readStorageIt;
	std::map<ap_uint<16>, ap_uint<8>*>::iterator writeStorageIt;
};

template <int WIDTH>
void dummyMemory<WIDTH>::setReadCmd(mmCmd cmd)
{
	// Address format from rx_engine.cpp:
	// pkgAddr[31:30] = 0, pkgAddr[29:WINDOW_BITS] = sessionID, pkgAddr[WINDOW_BITS-1:0] = offset
	readAddr = cmd.saddr(WINDOW_BITS-1, 0);
	readId = cmd.saddr(29, WINDOW_BITS);
	uint32_t tempLen = (uint32_t) cmd.bbt;
	readLen = (int) tempLen;
	//std::cout << "READ command: sessionID=" << std::hex << readId << " addr=" << readAddr << " len=" << std::dec << readLen << std::endl;
}

template <int WIDTH>
void dummyMemory<WIDTH>::setWriteCmd(mmCmd cmd)
{
	// Address format from rx_engine.cpp:
	// pkgAddr[31:30] = 0, pkgAddr[29:WINDOW_BITS] = sessionID, pkgAddr[WINDOW_BITS-1:0] = offset
	writeAddr = cmd.saddr(WINDOW_BITS-1, 0);
	writeId = cmd.saddr(29, WINDOW_BITS);
	uint32_t tempLen = (uint32_t) cmd.bbt;
	writeLen = (int) tempLen;
	writeCmdStartAddr = writeAddr;
	writeCmdLen = writeLen;
	writeCmdBytesWritten = 0;
	std::cerr << "[DDR setWriteCmd] sessionID=0x" << std::hex << (uint16_t)writeId
	          << " addr=0x" << (uint32_t)writeAddr
	          << " len=" << std::dec << (uint32_t)writeLen
	          << " (cmd.bbt=" << (uint32_t)cmd.bbt << ")" << std::endl;
}

template <int WIDTH>
void dummyMemory<WIDTH>::readWord(net_axis<WIDTH>& word)
{
	readStorageIt = storage.find(readId);
	if (readStorageIt == storage.end())
	{
		readStorageIt = createBuffer(readId);
		// check it?
	}
	int i = 0;
	word.keep = 0;
	while (readLen > 0 && i < (WIDTH/8))
	{
		word.data((i*8)+7, i*8) = (readStorageIt->second)[readAddr];
		word.keep = (word.keep << 1);
		word.keep++;
		readLen--;
		readAddr++;
		i++;
	}
	if (readLen == 0)
	{
		word.last = 1;
	}
	else
	{
		word.last = 0;
	}
}

template <int WIDTH>
void dummyMemory<WIDTH>::writeWord(net_axis<WIDTH>& word)
{
	writeStorageIt = storage.find(writeId);
	if (writeStorageIt == storage.end())
	{
		writeStorageIt = createBuffer(writeId);
	}
	//shuffleWord(word.data);
	int bytesThisWord = 0;
	for (int i = 0; i < (WIDTH/8); i++)
	{
		if (word.keep[i])
		{
			(writeStorageIt->second)[writeAddr] = word.data((i*8)+7, i*8);
			writeAddr++;
			writeLen--;
			bytesThisWord++;
			writeCmdBytesWritten++;
		}
		else
		{
			break;
		}
	}
	if (word.last)
	{
		if (writeLen != 0) {
			std::cerr << "[DDR ERROR] writeLen=" << std::dec << (uint32_t)writeLen
			          << " when word.last=1, writeId=0x" << std::hex << (uint16_t)writeId
			          << ", writeAddr=0x" << (uint32_t)writeAddr
			          << ", startAddr=0x" << (uint32_t)writeCmdStartAddr
			          << ", cmdLen=" << std::dec << (uint32_t)writeCmdLen
			          << ", totalBytesWritten=" << (uint32_t)writeCmdBytesWritten
			          << ", bytesThisWord=" << bytesThisWord << std::endl;
		}
		assert(writeLen == 0);
	}
}

template <int WIDTH>
std::map<ap_uint<16>, ap_uint<8>*>::iterator dummyMemory<WIDTH>::createBuffer(ap_uint<16> id)
{
	ap_uint<8>* array = new ap_uint<8>[BUFFER_SIZE]; // Match TOE BUFFER_SIZE (256KB with WINDOW_SCALE)
	std::pair<std::map<ap_uint<16>, ap_uint<8>*>::iterator, bool> ret;
	ret = storage.insert(std::make_pair(id, array));
	if (ret.second)
	{
		return ret.first;
	}
	return storage.end();
}

// Dump buffer contents for debugging
template <int WIDTH>
void dummyMemory<WIDTH>::dumpBuffer(ap_uint<16> sessionId, uint32_t startAddr, uint32_t length)
{
	auto it = storage.find(sessionId);
	if (it == storage.end()) {
		std::cerr << "[DDR Verify] Session 0x" << std::hex << sessionId << " not found" << std::endl;
		return;
	}
	std::cerr << "[DDR Verify] Session 0x" << std::hex << (uint16_t)sessionId
	          << " data at addr 0x" << startAddr << ", len=" << std::dec << length << ":" << std::endl;
	for (uint32_t i = 0; i < length; i++) {
		std::cerr << std::hex << std::setw(2) << std::setfill('0')
		          << (int)(uint8_t)(it->second)[startAddr + i] << " ";
		if ((i + 1) % 16 == 0) std::cerr << std::endl;
	}
	if (length % 16 != 0) std::cerr << std::endl;
}

// Verify data against expected values
template <int WIDTH>
bool dummyMemory<WIDTH>::verifyData(ap_uint<16> sessionId, uint32_t startAddr,
                                     const uint8_t* expectedData, uint32_t length)
{
	auto it = storage.find(sessionId);
	if (it == storage.end()) {
		std::cerr << "[DDR Verify] Session 0x" << std::hex << sessionId << " not found" << std::endl;
		return false;
	}
	for (uint32_t i = 0; i < length; i++) {
		uint8_t actual = (uint8_t)(it->second)[startAddr + i];
		if (actual != expectedData[i]) {
			std::cerr << "[DDR Verify] Mismatch at offset " << std::dec << i
			          << ": expected 0x" << std::hex << (int)expectedData[i]
			          << ", got 0x" << (int)actual << std::endl;
			return false;
		}
	}
	return true;
}

// Get list of session IDs in storage
template <int WIDTH>
std::vector<ap_uint<16>> dummyMemory<WIDTH>::getSessionIds()
{
	std::vector<ap_uint<16>> ids;
	for (auto& pair : storage) {
		ids.push_back(pair.first);
	}
	return ids;
}

#endif
