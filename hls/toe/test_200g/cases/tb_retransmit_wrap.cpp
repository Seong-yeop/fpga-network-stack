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
  if(toWrap>3*8960){write_data(b,8960,160);continue;}
  unsigned r=toWrap%8960;
  if(r>=64&&r<=8896)break;
  write_data(b,(r<64)?r+64:r-8896,160);
 }
 b.run(400);check(b.txPayload==b.expectedTx.size(),fmt("pre-wrap TX missing",b).c_str());
 unsigned start=b.expectedTx.size(),cross=to_wrap(b)/8960,base=b.ack+start;check(cross<4,"straddling segment outside the burst");
 b.autoAck=false;
 for(int i=0;i<6;i++)write_data(b,8960,0);
 unsigned before=b.readLog.size();
 for(unsigned k=2;k<=4;k++){wait_payload(b,start+k*8960,"window TX missing");send(b.ipRx,packet(b.seq,base,16,{},false,b.myPort));}
 unsigned limit=b.cycles+1500,keep=b.cycles;
 while((b.readLog.size()<before+5||b.txPayload<start+6*8960)&&b.cycles<limit){b.tick();if(b.cycles-keep>=60){send(b.ipRx,packet(b.seq,base,16,{},false,b.myPort));keep=b.cycles;}}
 check(b.txPayload==start+6*8960,fmt("window TX missing",b).c_str());
 check(b.readLog.size()==before+5,fmt("original burst must be four segments with one split read",b).c_str());
 unsigned expect=(base&(BUFFER_SIZE-1))|(b.sid<<WINDOW_BITS),covered=0,splits=0;
 for(unsigned i=before;i<before+5;i++){
  unsigned a=b.readLog[i].saddr.to_uint(),n=b.readLog[i].bbt.to_uint();
  check((a>>WINDOW_BITS)==b.sid,"retransmission read left the session ring");
  check(a==expect,"retransmission reads are not contiguous");
  check(((a&(BUFFER_SIZE-1))+n)<=BUFFER_SIZE,"retransmission read crosses the ring end");
  if(((a&(BUFFER_SIZE-1))+n)==BUFFER_SIZE)splits++;
  covered+=n;expect=(((a+n)&(BUFFER_SIZE-1))|(b.sid<<WINDOW_BITS));
 }
 check(covered==4*8960&&splits==1,fmt("burst did not cover four segments across the ring end",b).c_str());
 b.autoAck=true;send(b.ipRx,packet(b.seq,base+6*8960,16,{},false,b.myPort));b.run(2500);
 check(b.readLog.size()==before+5,fmt("buffer read after the cumulative ACK",b).c_str());
 check(b.txPayload==start+6*8960,fmt("payload after recovery",b).c_str());
}
int main(){return run_case("retransmit_wrap",test);}
