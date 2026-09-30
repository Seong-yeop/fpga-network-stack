#include "../common/csim.hpp"
static std::string fmt(const char *why,const Bench &b){return std::string(why)+" reads="+std::to_string(b.readLog.size())+" cycle="+std::to_string(b.cycles);}
static void test() {
 Bench b;connect_passive(b,1000,true,true);unsigned warm=warm_window(b,6),base=b.ack+warm;b.autoAck=false;
 for(int i=0;i<6;i++)write_data(b,8960,0);
 wait_tx(b,warm+6*8960,"initial TX missing");
 unsigned tA=b.cycles,before=b.readLog.size();
 send(b.ipRx,packet_sack(b.seq,base,{{base+8960,base+2*8960},{base+3*8960,base+4*8960},{base+5*8960,base+6*8960}}));
 wait_read(b,before+1,tA+sweeps(3),"three discontiguous SACK blocks must trigger");
 expect_read(b,before,base,8960,"retransmission read");
 b.run(300);
 check(b.readLog.size()==before+1,fmt("more than one segment retransmitted",b).c_str());
 send(b.ipRx,packet(b.seq,base+2*8960,16));b.run(400);
 check(b.readLog.size()==before+2,fmt("partial ACK must resend the second hole",b).c_str());
 expect_read(b,before+1,base+2*8960,8960,"second hole read");
 send(b.ipRx,packet(b.seq,base+4*8960,16));b.run(400);
 check(b.readLog.size()==before+3,fmt("partial ACK must resend the third hole",b).c_str());
 expect_read(b,before+2,base+4*8960,8960,"third hole read");
 b.autoAck=true;send(b.ipRx,packet(b.seq,base+6*8960,16));b.run(2500);
 check(b.readLog.size()==before+3,fmt("read after the cumulative ACK",b).c_str());
}
int main(){return run_case("sack_blocks",test);}
