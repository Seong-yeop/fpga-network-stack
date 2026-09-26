#pragma once
#include "../toe_top_decl.hpp"
#include "../../../ipv4/ipv4.hpp"
#include "../../two_complement_subchecksums.hpp"
#include "../../rx_sar_table/rx_sar_table.hpp"
#include <vector>
#include <deque>
#include <map>
#include <stdexcept>
#include <iostream>
#include <fstream>
using Bytes = std::vector<unsigned char>;
using Axis = ap_axiu<DATA_WIDTH,0,0,0>;
static void check(bool ok, const char *why) { if (!ok) throw std::runtime_error(why); }
static unsigned checksum(const Bytes &b) {
    unsigned sum=0;
    for(unsigned i=0;i<b.size();i+=2) sum+=(unsigned(b[i])<<8)|(i+1<b.size()?b[i+1]:0);
    while(sum>>16) sum=(sum&65535)+(sum>>16);
    return (~sum)&65535;
}
static void put(Bytes &b,unsigned at,unsigned v,unsigned n=2) { for(unsigned i=0;i<n;i++) b[at+i]=v>>(8*(n-1-i)); }
static unsigned get(const Bytes &b,unsigned at,unsigned n=2) { unsigned v=0; for(unsigned i=0;i<n;i++) v=(v<<8)|b.at(at+i); return v; }
static Bytes packet(unsigned seq,unsigned ack,unsigned flags,const Bytes &payload={}, bool options=false,unsigned dstPort=5001) {
    Bytes b(40+(options?8:0)+payload.size(),0); b[0]=0x45; put(b,2,b.size()); b[8]=64; b[9]=6;
    put(b,12,0x0a000002,4); put(b,16,0x0a000001,4); put(b,20,40000); put(b,22,dstPort);
    put(b,24,seq,4); put(b,28,ack,4); b[32]=(options?7:5)<<4; b[33]=flags; put(b,34,65535);
    if(options) {b[40]=2;b[41]=4;put(b,42,8960);b[44]=3;b[45]=3;b[46]=3;}
    std::copy(payload.begin(),payload.end(),b.end()-payload.size());
    Bytes pseudo(b.begin()+12,b.begin()+20); pseudo.push_back(0);pseudo.push_back(6);pseudo.push_back((b.size()-20)>>8);pseudo.push_back(b.size()-20);
    pseudo.insert(pseudo.end(),b.begin()+20,b.end()); put(b,36,checksum(pseudo));
    put(b,10,checksum(Bytes(b.begin(),b.begin()+20)));return b;
}
static void send(stream<Axis> &s,const Bytes &b) {
    for(unsigned o=0;o<b.size();o+=DATA_WIDTH/8) { Axis a{};a.data=0;a.keep=0;a.strb=0;for(unsigned j=0;j<DATA_WIDTH/8 && o+j<b.size();j++){a.data(j*8+7,j*8)=b[o+j];a.keep[j]=1;} a.strb=a.keep;a.last=o+DATA_WIDTH/8>=b.size();s.write(a); }
}
static void append(Bytes &b,const Axis &a) {for(unsigned j=0;j<DATA_WIDTH/8;j++) if(a.keep[j]) b.push_back(a.data(j*8+7,j*8));}
struct Bench {
#ifdef V80_RUNTIME_MSS
 ap_uint<16> max_mss=TCP_MSS;
#endif
 stream<Axis> ipRx,rxRead,txRead,ipTx,rxWrite,txWrite,appTx,appRx;
 stream<mmStatus> txStatus; stream<mmCmd> txWC,txRC;
 stream<rtlSessionLookupReply> lr;stream<rtlSessionUpdateReply> ur;
 stream<rtlSessionLookupRequest> lq;stream<rtlSessionUpdateRequest> uq;
 stream<ap_uint<16>> listen,close,rxMeta;stream<appReadRequest> read;
 stream<ipTuple> open;stream<appTxMeta> txMeta;stream<bool> listened;
 stream<appNotification> notices;stream<openStatus> opened;stream<appTxRsp> txRsp;
 ap_uint<16> count=0;unsigned cycles=0,seq=1001,ack=0,sid=0,myPort=5001; bool synack=false, expectWS=true, autoAck=true;unsigned delay=1, openReplies=0, writePauseUntil=0, readPauseUntil=0, readCommands=0;std::deque<Bytes> packets;
 std::map<threeTupleInternal,unsigned> table;std::map<unsigned,unsigned char> memory;
 std::deque<unsigned> pendingLengths, lengths;std::deque<Axis> fifo;std::deque<mmCmd> writes,reads; unsigned wo=0,ro=0;
#if !(RX_DDR_BYPASS)
 stream<mmStatus> rxStatus; stream<mmCmd> rxWC,rxRC; std::map<unsigned,unsigned char> rxmem; std::deque<mmCmd> rxWrites,rxReads; unsigned rwo=0,rro=0; std::deque<std::pair<unsigned,mmStatus>> rxStatusQ;
 static unsigned ring(unsigned base,unsigned i){return (base&~(BUFFER_SIZE-1))|((base+i)&(BUFFER_SIZE-1));}
#endif
 Bytes out,received,expectedTx;unsigned txPayload=0;bool fin=false;
 void tick() {
#if RX_DDR_BYPASS
  toe(ipRx,txStatus,rxRead,txRead,ipTx,txWC,txRC,rxWrite,txWrite,lr,ur,lq,uq,listen,read,open,close,txMeta,appTx,listened,notices,rxMeta,appRx,opened,txRsp,ap_uint<16>(fifo.size()),ap_uint<16>(8192),0x0100000a,count
#ifdef V80_RUNTIME_MSS
,max_mss
#endif
);
#else
  toe(ipRx,rxStatus,txStatus,rxRead,txRead,ipTx,rxWC,rxRC,txWC,txRC,rxWrite,txWrite,lr,ur,lq,uq,listen,read,open,close,txMeta,appTx,listened,notices,rxMeta,appRx,opened,txRsp,0x0100000a,count
#ifdef V80_RUNTIME_MSS
,max_mss
#endif
);
#endif
  cycles++;
  if(cycles%delay==0 && !lq.empty()) {auto q=lq.read();auto it=table.find(q.key);rtlSessionLookupReply r(it!=table.end(),it==table.end()?0:it->second,q.source);r.key=q.key;lr.write(r);}
  if(cycles%delay==0 && !uq.empty()) {auto q=uq.read();if(q.op==INSERT)table[q.key]=q.value;else table.erase(q.key);rtlSessionUpdateReply r(q.value,q.op,q.source);r.key=q.key;r.success=true;ur.write(r);}
#if RX_DDR_BYPASS
  if(!rxWrite.empty()){check(fifo.size()<8192,"RX FIFO overflow");fifo.push_back(rxWrite.read());}
  if(!fifo.empty() && rxRead.empty()){rxRead.write(fifo.front());fifo.pop_front();}
#else
  if(!rxWC.empty())rxWrites.push_back(rxWC.read());
  if(!rxWrites.empty()&&!rxWrite.empty()){auto a=rxWrite.read();unsigned base=rxWrites.front().saddr.to_uint();for(unsigned j=0;j<DATA_WIDTH/8;j++)if(a.keep[j])rxmem[ring(base,rwo++)]=a.data(j*8+7,j*8);if(a.last){check(rwo==rxWrites.front().bbt,"RX write length");rxWrites.pop_front();rwo=0;mmStatus s{};s.okay=1;rxStatusQ.push_back({cycles+8,s});}}
  if(!rxStatusQ.empty()&&rxStatusQ.front().first<=cycles){rxStatus.write(rxStatusQ.front().second);rxStatusQ.pop_front();}
  if(!rxRC.empty())rxReads.push_back(rxRC.read());
  if(cycles>=readPauseUntil&&!rxReads.empty()&&rxRead.empty()){Axis a{};a.data=0;a.keep=0;a.strb=0;auto c=rxReads.front();unsigned base=c.saddr.to_uint();for(unsigned j=0;j<DATA_WIDTH/8&&rro<c.bbt;j++,rro++){a.data(j*8+7,j*8)=rxmem.at(ring(base,rro));a.keep[j]=1;}a.strb=a.keep;a.last=rro==c.bbt;rxRead.write(a);if(a.last){rxReads.pop_front();rro=0;}}
#endif
  if(cycles%delay==0 && !notices.empty()){auto n=notices.read();sid=n.sessionID;if(n.length){read.write(appReadRequest(n.sessionID,n.length));pendingLengths.push_back(n.length);}}
  if(!rxMeta.empty()){rxMeta.read();check(!pendingLengths.empty(),"unexpected RX metadata");lengths.push_back(pendingLengths.front());pendingLengths.pop_front();}
  if(cycles%delay==0 && !appRx.empty()){auto a=appRx.read();check(!lengths.empty(),"missing RX metadata");unsigned before=received.size();append(received,a);unsigned bytes=received.size()-before;check(bytes<=lengths.front(),"RX request length exceeded");lengths.front()-=bytes;if(a.last){check(lengths.front()==0,"early RX TLAST");lengths.pop_front();}else check(lengths.front()!=0,"missing RX TLAST");}
  if(!txWC.empty())writes.push_back(txWC.read());
  if(cycles>=writePauseUntil && cycles%delay==0 && !writes.empty()&&!txWrite.empty()){auto a=txWrite.read();for(unsigned j=0;j<DATA_WIDTH/8;j++)if(a.keep[j])memory[writes.front().saddr.to_uint()+wo++]=a.data(j*8+7,j*8);if(a.last){check(wo==writes.front().bbt,"write length");writes.pop_front();wo=0;mmStatus s{};s.okay=1;txStatus.write(s);}}
  if(!txRC.empty()){reads.push_back(txRC.read());readCommands++;}
  if(cycles%delay==0 && !reads.empty()&&txRead.empty()){Axis a{};a.data=0;a.keep=0;a.strb=0;auto c=reads.front();for(unsigned j=0;j<DATA_WIDTH/8 && ro<c.bbt;j++,ro++){a.data(j*8+7,j*8)=memory.at(c.saddr.to_uint()+ro);a.keep[j]=1;}a.strb=a.keep;a.last=ro==c.bbt;txRead.write(a);if(a.last){reads.pop_front();ro=0;}}
  if(!txRsp.empty())check(txRsp.read().error==0,"TX request rejected");
  if(!opened.empty()){auto r=opened.read();check(r.success,"open failed");sid=r.sessionID;openReplies++;}
  if(!ipTx.empty()){auto a=ipTx.read();append(out,a);if(a.last){check(out.size()>=40,"short TCP packet");check(get(out,10)==0,"IP checksum placeholder");Bytes p(out.begin()+12,out.begin()+20);p.push_back(0);p.push_back(6);p.push_back((out.size()-20)>>8);p.push_back(out.size()-20);p.insert(p.end(),out.begin()+20,out.end());check(checksum(p)==0,"TCP checksum");if(out[33]&2){check(out.size()==(expectWS?48:44),"SYN option length");check(out[40]==2&&get(out,42)==8960,"SYN MSS option");if(expectWS)check(out[44]==3&&out[46]==3,"SYN WS option");ack=get(out,24,4)+1;myPort=get(out,20);synack=true;} unsigned h=20+(out[32]>>4)*4; if(out[33]&1){fin=true;send(ipRx,packet(seq,get(out,24,4)+1,16,{},false,myPort));} if(out.size()>h){unsigned offset=get(out,24,4)-ack;check(offset+out.size()-h<=expectedTx.size(),"TX unexpected bytes");for(unsigned i=h;i<out.size();i++)check(out[i]==expectedTx[offset+i-h],"TX payload mismatch");txPayload=std::max(txPayload,offset+unsigned(out.size()-h));if(autoAck)send(ipRx,packet(seq,get(out,24,4)+out.size()-h,16,{},false,myPort));}packets.push_back(out);out.clear();}}
 }
 void run(unsigned n){for(unsigned i=0;i<n;i++)tick();}
};

static Bytes resign(Bytes b) {
 put(b,36,0); put(b,10,0); put(b,2,b.size());
 Bytes p(b.begin()+12,b.begin()+20);p.push_back(0);p.push_back(6);p.push_back((b.size()-20)>>8);p.push_back(b.size()-20);
 p.insert(p.end(),b.begin()+20,b.end());put(b,36,checksum(p));put(b,10,checksum(Bytes(b.begin(),b.begin()+20)));return b;
}
static void connect_passive(Bench &b,unsigned isn=1000,bool ws=true) {
 b.expectWS=ws;b.seq=isn+1;b.listen.write(5001);b.run(40000);check(!b.listened.empty()&&b.listened.read(),"listen timeout");
 Bytes syn=packet(isn,0,2,{},true);if(!ws){syn.resize(44);syn[32]=6<<4;syn=resign(syn);}send(b.ipRx,syn);
 b.run(1000);check(b.synack,"SYN-ACK timeout");send(b.ipRx,packet(b.seq,b.ack,16));b.run(300);b.packets.clear();
}
static void receive_data(Bench &b,unsigned n) {
 Bytes data(n);for(unsigned i=0;i<n;i++)data[i]=(b.received.size()+i)*17+5;
 Bytes expected=b.received;expected.insert(expected.end(),data.begin(),data.end());send(b.ipRx,packet(b.seq,b.ack,24,data,false,b.myPort));b.seq+=n;b.run(1500);check(b.received==expected,"RX bytes differ");
}
static void transmit_data(Bench &b,unsigned n) {
 Bytes data(n);for(unsigned i=0;i<n;i++)data[i]=(b.expectedTx.size()+i)*31+9;
 b.expectedTx.insert(b.expectedTx.end(),data.begin(),data.end());b.txMeta.write(appTxMeta(b.sid,n));b.run(20);send(b.appTx,data);b.run(1500);check(b.txPayload==b.expectedTx.size(),"TX bytes missing");
}
static int run_case(const char *name,void (*test)()) {
 try {test();std::cout<<std::dec<<"PASS "<<name<<" width="<<DATA_WIDTH<<std::endl;return 0;}
 catch(const std::exception &e){std::cerr<<"FAIL "<<name<<": "<<e.what()<<std::endl;return 1;}
}
