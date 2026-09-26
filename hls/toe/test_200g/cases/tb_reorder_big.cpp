#include "../common/csim.hpp"
static void test() {
 Bench b;connect_passive(b);receive_data(b,8960);Bytes expected=b.received;
#if !(RX_DDR_BYPASS)
 const unsigned n=8960,late=1,following=18;
 Bytes data[following+1];
 for(unsigned s=0;s<=following;s++){data[s].resize(n);for(unsigned i=0;i<n;i++)data[s][i]=(expected.size()+s*n+i)*17+5;}
 for(unsigned s=1;s<=following;s++){send(b.ipRx,packet(b.seq+s*n,b.ack,24,data[s],false,b.myPort));b.run(400);}
 check(b.received==expected,"out-of-order data delivered early");
 send(b.ipRx,packet(b.seq,b.ack,24,data[0],false,b.myPort));b.run(6000);
 for(unsigned s=0;s<=following;s++)expected.insert(expected.end(),data[s].begin(),data[s].end());
 check(b.received.size()==expected.size(),"reassembled size");
 check(b.received==expected,"reassembled data");
 b.seq+=(following+1)*n;
#endif
 receive_data(b,8960);receive_data(b,128);
}
int main(){return run_case("reorder_big",test);}
