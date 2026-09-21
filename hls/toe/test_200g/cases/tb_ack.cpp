// Scenarios: Linux packetdrill ACK boundaries; lwIP sequence tests.
#include "../common/csim.hpp"
static void test() {
 Bench b;connect_passive(b);b.autoAck=false;transmit_data(b,129);
 // Future and stale ACKs must not prevent subsequent valid traffic.
 send(b.ipRx,packet(b.seq,b.ack+10000,16));b.run(100);
 send(b.ipRx,packet(b.seq,b.ack-1,16));b.run(100);
 for(unsigned partial:{1u,63u,64u,65u,127u,128u,129u}) {
  send(b.ipRx,packet(b.seq,b.ack+partial,16));b.run(50);
 }
 b.autoAck=true;transmit_data(b,256);receive_data(b,129);
}
int main(){return run_case("ack",test);}
