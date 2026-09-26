#include "../common/csim.hpp"
static unsigned last_window(Bench &b) {
 for(auto it=b.packets.rbegin();it!=b.packets.rend();++it)if(it->size()>=40&&!((*it)[33]&2))return get(*it,34)<<3;
 return 0;
}
static void test() {
 Bench b;connect_passive(b);receive_data(b,8960);
#if !(RX_DDR_BYPASS)
 const unsigned n=8960,segs=8;
 Bytes expected=b.received;
 b.readPauseUntil=~0u;
 for(unsigned s=0;s<segs;s++){Bytes d(n);for(unsigned i=0;i<n;i++)d[i]=(expected.size()+i)*13+1;expected.insert(expected.end(),d.begin(),d.end());send(b.ipRx,packet(b.seq,b.ack,24,d,false,b.myPort));b.seq+=n;b.run(600);}
 unsigned held=last_window(b);
 check(held<=(1u<<18)-1-segs*n,"window reopened before the read data left the ring");
 b.readPauseUntil=0;b.run(6000);
 check(b.received==expected,"data after the reads resumed");
 receive_data(b,128);
 check(last_window(b)+256>=(1u<<18)-8,"window not restored after the reads completed");
#endif
}
int main(){return run_case("window_release",test);}
