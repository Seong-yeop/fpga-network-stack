#include "../common/csim.hpp"
static std::string fmt(const char *why,const Bench &b){return std::string(why)+" txPayload="+std::to_string(b.txPayload)+" reads="+std::to_string(b.readLog.size())+" cycle="+std::to_string(b.cycles);}
static void wait_payload(Bench &b,unsigned bytes,const char *why){unsigned limit=b.cycles+4000;while(b.txPayload<bytes){check(b.cycles<limit,fmt(why,b).c_str());b.tick();}}
static unsigned to_wrap(const Bench &b){return BUFFER_SIZE-((b.ack+b.expectedTx.size())&(BUFFER_SIZE-1));}
static void test() {
 Bench b;connect_passive(b);check(b.sid==0,"first session id");
 ipTuple target;target.ip_address=0x0a000002;target.ip_port=40000;b.open.write(target);b.run(300);
 check(!b.packets.empty(),"active SYN missing");Bytes syn=b.packets.back();check(syn[33]==2,"active open did not send SYN");
 unsigned isn=get(syn,24,4);
 Bytes reply=packet(1000,isn+1,18,{},true);put(reply,22,get(syn,20));reply=resign(reply);send(b.ipRx,reply);b.run(1000);
 check(b.openReplies==1,"open result missing");check(b.sid==1,"second session id");
 b.seq=1001;b.ack=isn+1;b.packets.clear();
 warm_window(b,6);
 for(;;){
  unsigned toWrap=to_wrap(b);
  if(toWrap>8960){write_data(b,8960,160);continue;}
  if(toWrap>=64&&toWrap<=8896)break;
  write_data(b,(toWrap<64)?toWrap+64:toWrap-8896,160);
 }
 b.run(400);check(b.txPayload==b.expectedTx.size(),fmt("pre-wrap TX missing",b).c_str());
 unsigned start=b.expectedTx.size(),base=b.ack+start,firstLen=to_wrap(b);unsigned tA=b.cycles;
 check(firstLen>=64&&firstLen<8960,"first unacknowledged segment does not straddle the ring end");
 b.autoAck=false;
 for(int i=0;i<6;i++)write_data(b,8960,0);
 unsigned before=b.readLog.size();
 for(unsigned k=2;k<=4;k++){wait_payload(b,start+k*8960,"window TX missing");send(b.ipRx,packet(b.seq,base,16,{},false,b.myPort));}
 unsigned limit=b.cycles+1500,keep=b.cycles;
 while((b.readLog.size()<before+2||b.txPayload<start+6*8960)&&b.cycles<limit){b.tick();if(b.cycles-keep>=60){send(b.ipRx,packet(b.seq,base,16,{},false,b.myPort));keep=b.cycles;}}
 check(b.cycles-tA<sweeps(12),"case ran into the RTO window");
 check(b.txPayload==start+6*8960,fmt("window TX missing",b).c_str());
 check(b.readLog.size()==before+2,fmt("straddling retransmission must be two reads",b).c_str());
 unsigned a0=b.readLog[before].saddr.to_uint(),n0=b.readLog[before].bbt.to_uint();
 unsigned a1=b.readLog[before+1].saddr.to_uint(),n1=b.readLog[before+1].bbt.to_uint();
 check((a0>>WINDOW_BITS)==b.sid&&(a1>>WINDOW_BITS)==b.sid,"retransmission read left the session ring");
 check((a0&(BUFFER_SIZE-1))==(base&(BUFFER_SIZE-1)),"first half address");
 check(n0==firstLen&&((a0&(BUFFER_SIZE-1))+n0)==BUFFER_SIZE,"first half must end at the ring end");
 check((a1&(BUFFER_SIZE-1))==0&&n1==8960-firstLen,"second half must start the session ring");
 b.autoAck=true;send(b.ipRx,packet(b.seq,base+6*8960,16,{},false,b.myPort));b.run(2500);
 check(b.readLog.size()==before+2,fmt("buffer read after the cumulative ACK",b).c_str());
 check(b.txPayload==start+6*8960,fmt("payload after recovery",b).c_str());
}
int main(){return run_case("retransmit_wrap",test);}
