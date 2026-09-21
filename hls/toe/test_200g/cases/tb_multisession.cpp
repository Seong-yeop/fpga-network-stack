// Session lookup/update responses may be delayed, but must retain tuple ownership.
#include "../common/csim.hpp"
static void test() {
 Bench b;b.delay=7;b.listen.write(5001);b.run(40000);check(!b.listened.empty()&&b.listened.read(),"listen");
 unsigned ack[8];
 for(unsigned id=0;id<8;id++) {
  Bytes syn=packet(1000,0,2,{},true);put(syn,20,40000+id);send(b.ipRx,resign(syn));b.run(500);
  check(!b.packets.empty(),"SYN-ACK missing");Bytes response=b.packets.back();check(get(response,22)==40000+id,"wrong SYN-ACK tuple");ack[id]=get(response,24,4)+1;
  Bytes a=packet(1001,ack[id],16);put(a,20,40000+id);send(b.ipRx,resign(a));b.run(100);
 }
 Bytes expected;
 for(unsigned id=0;id<8;id++) {
  Bytes data(65+id,id+1);expected.insert(expected.end(),data.begin(),data.end());Bytes p=packet(1001,ack[id],24,data);put(p,20,40000+id);send(b.ipRx,resign(p));
 }
 b.run(3000);check(b.received==expected,"cross-session RX mismatch");check(b.table.size()==8,"session mapping count");
}
int main(){return run_case("multisession",test);}
