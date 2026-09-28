// Regression: a retransmission whose window straddles the 256 KiB ring boundary of a session other
// than 0 must read both halves from that session's ring (the wrapped half used to start at address 0).
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
 b.run(400);check(b.txPayload==b.expectedTx.size(),fmt("pre-wrap TX missing",b).c_str());
 b.autoAck=false;unsigned start=b.expectedTx.size();
 for(int i=0;i<7;i++)write_segment(b,8960,150);
 b.run(200);check(b.txPayload==start+7*8960,fmt("window TX missing",b).c_str());
 unsigned acked=b.ack+start+8960;send(b.ipRx,packet(b.seq,acked,16,{},false,b.myPort));b.run(30);
 unsigned before=b.readLog.size();
 for(int i=0;i<3;i++){send(b.ipRx,packet(b.seq,acked,16,{},false,b.myPort));b.run(10);}
 b.run(400);
 check(b.readLog.size()>before,fmt("no retransmission reads",b).c_str());
 unsigned expect=((isn+1+start+8960)&(BUFFER_SIZE-1))|(b.sid<<WINDOW_BITS),covered=0;
 for(unsigned i=before;i<b.readLog.size()&&covered<6*8960;i++){
  unsigned a=b.readLog[i].saddr.to_uint(),n=b.readLog[i].bbt.to_uint();
  check((a>>WINDOW_BITS)==b.sid,"retransmission read left the session ring");
  check(a==expect,"retransmission reads are not contiguous");
  check(((a&(BUFFER_SIZE-1))+n)<=BUFFER_SIZE,"retransmission read crosses the ring end");
  covered+=n;expect=(((a+n)&(BUFFER_SIZE-1))|(b.sid<<WINDOW_BITS));
 }
 check(covered==6*8960,fmt("retransmission did not cover the unacked window",b).c_str());
 b.autoAck=true;send(b.ipRx,packet(b.seq,acked,16,{},false,b.myPort));b.run(2500);
 check(b.txPayload==start+7*8960,fmt("payload after recovery",b).c_str());
}
int main(){return run_case("retransmit_wrap",test);}
