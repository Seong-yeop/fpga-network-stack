#include "../common/csim.hpp"
#ifndef V80_RUNTIME_MSS
#error This test requires the network TOE variant
#endif
static void test(){
 Bench b;connect_passive(b);b.max_mss=1460;b.autoAck=false;
 for(int i=0;i<3;i++)transmit_data(b,1460);
 b.packets.clear();
 for(int i=0;i<4;i++)send(b.ipRx,packet(b.seq,b.ack,16));
 b.run(5000);check(b.readCommands>0,"small MSS retransmission not exercised");
 bool data=false;
 for(auto& p:b.packets){unsigned header=20+(p[32]>>4)*4;
  if(p.size()>header){data=true;check(p.size()-header<=1460,"retransmission exceeded peer MSS");}
 }
 check(data,"missing retransmitted payload");
}
int main(){return run_case("runtime_mss",test);}
