#include "../common/csim.hpp"
static void test() {
 Bench b;b.listen.write(5001);b.run(40000);check(!b.listened.empty()&&b.listened.read(),"listen");
 unsigned ack0=0;
 for(unsigned id=0;id<MAX_SESSIONS;id++) {
  Bytes syn=packet(1000,0,2,{},true);put(syn,20,40000+id);send(b.ipRx,resign(syn));b.run(500);
  check(!b.packets.empty(),"SYN-ACK missing");Bytes response=b.packets.back();
  check((response[33]&0x16)==0x12&&get(response,22)==40000+id,"SYN-ACK flags or tuple");
  unsigned ack=get(response,24,4)+1;if(id==0)ack0=ack;
  Bytes a=packet(1001,ack,16);put(a,20,40000+id);send(b.ipRx,resign(a));b.run(60);
 }
 check(b.table.size()==MAX_SESSIONS,"session count");
 b.packets.clear();
 Bytes syn=packet(1000,0,2,{},true);put(syn,20,40000+MAX_SESSIONS);send(b.ipRx,resign(syn));b.run(500);
 check(!b.packets.empty(),"no answer to the SYN above the session limit");
 Bytes rst=b.packets.back();
 check((rst[33]&0x17)==0x14&&get(rst,22)==40000+MAX_SESSIONS,"RST+ACK expected for the SYN above the session limit");
 check(get(rst,28,4)==1001,"RST acknowledges SYN sequence + 1");
 check(b.table.size()==MAX_SESSIONS,"session count after the refused SYN");
 b.packets.clear();
 send(b.ipRx,resign(syn));b.run(500);
 check(!b.packets.empty()&&(b.packets.back()[33]&0x04),"second SYN above the limit not reset");
 b.ack=ack0;receive_data(b,65);
 check(b.table.size()==MAX_SESSIONS,"session count after data on session 0");
}
int main(){return run_case("syn_exhaust",test);}
