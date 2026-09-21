// RX_DDR_BYPASS drops out-of-order/overlapping segments; retransmit to recover.
#include "../common/csim.hpp"
static void test() {
 Bench b;connect_passive(b);receive_data(b,128);Bytes original=b.received;
 send(b.ipRx,packet(b.seq+128,b.ack,24,Bytes(128,0x33)));b.run(500);
 send(b.ipRx,packet(b.seq-128,b.ack,24,original));b.run(500);
 send(b.ipRx,packet(b.seq-64,b.ack,24,Bytes(128,0x55)));b.run(500);
 check(b.received==original,"duplicate/out-of-order data delivered");
 receive_data(b,128);receive_data(b,128);
}
int main(){return run_case("reorder",test);}
