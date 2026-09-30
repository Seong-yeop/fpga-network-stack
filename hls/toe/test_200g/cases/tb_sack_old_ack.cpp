#include "../common/csim.hpp"
static std::string fmt(const char *why,const Bench &b){return std::string(why)+" reads="+std::to_string(b.readLog.size())+" cycle="+std::to_string(b.cycles);}
static void test() {
 Bench b;connect_passive(b,1000,true,true);unsigned warm=warm_window(b,6),base=b.ack+warm;b.autoAck=false;
 for(int i=0;i<6;i++)write_data(b,8960,0);
 wait_tx(b,warm+6*8960,"initial TX missing");
 unsigned before=b.readLog.size();
 send(b.ipRx,packet(b.seq,base+2*8960,16));b.run(200);
 unsigned tA=b.cycles;
 for(int i=0;i<3;i++){send(b.ipRx,packet_sack(b.seq,base,{{base+3*8960,base+6*8960}}));b.run(100);}
 b.run(400);
 check(b.cycles-tA<sweeps(12),"case ran into the RTO window");
 check(b.readLog.size()==before,fmt("SACK blocks on an old ACK triggered a retransmission",b).c_str());
 send(b.ipRx,packet_sack(b.seq,base+2*8960,{{base+3*8960,base+6*8960}}));
 wait_read(b,before+1,tA+sweeps(6),"SACK block on the current ACK did not trigger");
 expect_read(b,before,base+2*8960,8960,"retransmission read");
 b.autoAck=true;send(b.ipRx,packet(b.seq,base+6*8960,16));b.run(2500);
 check(b.readLog.size()==before+1,fmt("buffer read after the cumulative ACK",b).c_str());
}
int main(){return run_case("sack_old_ack",test);}
