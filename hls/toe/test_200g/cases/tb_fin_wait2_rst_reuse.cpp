#include "../common/csim.hpp"
static void test() {
 Bench b;b.listen.write(5001);b.run(40000);check(!b.listened.empty()&&b.listened.read(),"listen");
 unsigned ack0=0;
 for(unsigned id=0;id<MAX_SESSIONS;id++) {
  Bytes syn=packet(1000,0,2,{},true);put(syn,20,40000+id);send(b.ipRx,resign(syn));b.run(500);
  check(!b.packets.empty()&&(b.packets.back()[33]&0x12)==0x12,"SYN-ACK missing");
  unsigned ack=get(b.packets.back(),24,4)+1;if(id==0)ack0=ack;
  Bytes a=packet(1001,ack,16);put(a,20,40000+id);send(b.ipRx,resign(a));b.run(60);
 }
 check(b.table.size()==MAX_SESSIONS,"session count");
 peerPort=40000;b.seq=1001;b.ack=ack0;receive_data(b,65);
 unsigned c=b.closeNotices,sid0=b.sid;
 b.close.write(sid0);b.run(500);check(b.fin&&b.finCount==1,"local FIN on session 0");
 b.ack+=1;unsigned tA=b.cycles;
 send(b.ipRx,packet(b.seq,b.ack,4));b.run(500);
 check(b.closeNotices==c+1&&b.table.size()==MAX_SESSIONS-1,"RST after the FIN ACK");
 reopen(b,5000,40000+MAX_SESSIONS);
 check(b.table.size()==MAX_SESSIONS,"reconnect reusing the released session");
 receive_data(b,65);check(b.sid==sid0,"released session ID reused");
 while(b.cycles<tA+sweeps(66))b.tick();
 check(b.table.size()==MAX_SESSIONS,"stale close timer released the reused session");
 receive_data(b,65);
 check(b.closeNotices==c+1,"stale close timer notification");
}
int main(){return run_case("fin_wait2_rst_reuse",test);}
