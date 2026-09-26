// RX_DDR_BYPASS drops out-of-order/overlapping segments; retransmit to recover.
#include "../common/csim.hpp"
static void test() {
 Bench b;connect_passive(b);receive_data(b,128);Bytes original=b.received;
#if RX_DDR_BYPASS
 send(b.ipRx,packet(b.seq+128,b.ack,24,Bytes(128,0x33)));b.run(500);
 send(b.ipRx,packet(b.seq-128,b.ack,24,original));b.run(500);
 send(b.ipRx,packet(b.seq-64,b.ack,24,Bytes(128,0x55)));b.run(500);
 check(b.received==original,"duplicate/out-of-order data delivered");
#else
 Bytes late(128,0x55),early(128,0x33);
 send(b.ipRx,packet(b.seq+128,b.ack,24,early));b.run(500);check(b.received==original,"out-of-order data delivered early");
 send(b.ipRx,packet(b.seq,b.ack,24,late));b.run(1500);
 Bytes expected=original;expected.insert(expected.end(),late.begin(),late.end());expected.insert(expected.end(),early.begin(),early.end());
 check(b.received==expected,"reassembled data");b.seq+=256;
 send(b.ipRx,packet(b.seq-128,b.ack,24,early));b.run(500);check(b.received==expected,"duplicate delivered");
#endif
 receive_data(b,128);receive_data(b,128);
}
int main(){return run_case("reorder",test);}
