// Scapy inet.uts: bad checksum and invalid TCP data offset, then a good packet.
#include "../common/csim.hpp"
static void test() {
 Bench b;connect_passive(b);
 Bytes bad=packet(b.seq,b.ack,24,Bytes(65,0x5a));bad[36]^=1;
 send(b.ipRx,bad);b.run(500);check(b.received.empty(),"bad TCP checksum delivered");receive_data(b,65);
 bad=packet(b.seq,b.ack,24,Bytes(65,0x5a));bad[32]=4<<4;bad=resign(bad);
 send(b.ipRx,bad);b.run(500);check(b.received.size()==65,"invalid TCP header delivered");receive_data(b,129);
}
int main(){return run_case("errors",test);}
