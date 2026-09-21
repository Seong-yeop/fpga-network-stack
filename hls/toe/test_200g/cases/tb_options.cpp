// MSS and Window Scale: absent, zero, and positive negotiated scale.
#include "../common/csim.hpp"
#include <cstdlib>
static void test() {
 int ws=std::getenv("TOE_WS")?std::atoi(std::getenv("TOE_WS")):-1;
 Bench b;b.expectWS=ws>0;b.listen.write(5001);b.run(40000);
 Bytes syn=packet(1000,0,2,{},true);
 if(ws<0){syn.resize(44);syn[32]=6<<4;}else syn[46]=ws;
 send(b.ipRx,resign(syn));b.run(1000);check(b.synack,"SYN-ACK timeout");
 send(b.ipRx,packet(b.seq,b.ack,16));b.run(300);receive_data(b,129);transmit_data(b,8960);
}
int main(){return run_case("options",test);}
