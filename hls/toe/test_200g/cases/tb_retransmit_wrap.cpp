// Regression: a retransmitted segment that straddles the 256 KiB ring boundary of a session other than 0
// must be read in two parts from that session's ring (the wrapped part used to start at address 0).
#include "../common/csim.hpp"
static std::string fmt(const char *why,const Bench &b){return std::string(why)+" txPayload="+std::to_string(b.txPayload)+" reads="+std::to_string(b.readLog.size())+" cycle="+std::to_string(b.cycles);}
static void write_segment(Bench &b,unsigned n,unsigned settle) {
 Bytes data(n);for(unsigned k=0;k<n;k++)data[k]=(b.expectedTx.size()+k)*31+9;
 b.expectedTx.insert(b.expectedTx.end(),data.begin(),data.end());
 b.txMeta.write(appTxMeta(b.sid,n));b.run(20);send(b.appTx,data);b.run(settle);
}
static void test() {
 Bench b;connect_passive(b);check(b.sid==0,"first session id");
 ipTuple target;target.ip_address=0x0a000002;target.ip_port=40000;b.open.write(target);b.run(300);
 check(!b.packets.empty(),"active SYN missing");Bytes syn=b.packets.back();check(syn[33]==2,"active open did not send SYN");
 unsigned isn=get(syn,24,4);
 Bytes reply=packet(1000,isn+1,18,{},true);put(reply,22,get(syn,20));reply=resign(reply);send(b.ipRx,reply);b.run(1000);
 check(b.openReplies==1,"open result missing");check(b.sid==1,"second session id");
 b.seq=1001;b.ack=isn+1;b.packets.clear();
 unsigned toWrap=BUFFER_SIZE-((isn+1)&(BUFFER_SIZE-1));
 while(toWrap>4*8960){write_segment(b,8960,160);toWrap-=8960;}
 if(toWrap%8960<64){write_segment(b,100,160);toWrap-=100;}
 b.run(400);check(b.txPayload==b.expectedTx.size(),fmt("pre-wrap TX missing",b).c_str());
 b.autoAck=false;unsigned start=b.expectedTx.size(),cross=toWrap/8960;
 for(int i=0;i<7;i++)write_segment(b,8960,150);
 b.run(200);check(b.txPayload==start+7*8960,fmt("window TX missing",b).c_str());
 unsigned hole=b.ack+start+cross*8960;send(b.ipRx,packet(b.seq,hole,16,{},false,b.myPort));b.run(30);
 unsigned before=b.readLog.size();
 for(int i=0;i<3;i++){send(b.ipRx,packet(b.seq,hole,16,{},false,b.myPort));b.run(10);}
 b.run(400);
 check(b.readLog.size()==before+2,fmt("a segment across the ring end must be read in exactly two parts",b).c_str());
 unsigned first=b.readLog[before].saddr.to_uint(),n1=b.readLog[before].bbt.to_uint();
 unsigned second=b.readLog[before+1].saddr.to_uint(),n2=b.readLog[before+1].bbt.to_uint();
 check((first>>WINDOW_BITS)==b.sid&&(second>>WINDOW_BITS)==b.sid,"retransmission read left the session ring");
 check((first&(BUFFER_SIZE-1))==(hole&(BUFFER_SIZE-1)),"first part does not start at the hole");
 check((first&(BUFFER_SIZE-1))+n1==BUFFER_SIZE,"first part does not end at the ring end");
 check((second&(BUFFER_SIZE-1))==0&&n1+n2==8960,"second part does not continue at the ring start");
 b.autoAck=true;send(b.ipRx,packet(b.seq,b.ack+start+7*8960,16,{},false,b.myPort));b.run(2500);
 check(b.readLog.size()==before+2,fmt("buffer read after the cumulative ACK",b).c_str());
 check(b.txPayload==start+7*8960,fmt("payload after recovery",b).c_str());
}
int main(){return run_case("retransmit_wrap",test);}
