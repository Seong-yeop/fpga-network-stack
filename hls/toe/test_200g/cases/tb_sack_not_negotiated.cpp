#include "../common/csim.hpp"
static std::string fmt(const char *why,const Bench &b){return std::string(why)+" reads="+std::to_string(b.readLog.size())+" cycle="+std::to_string(b.cycles);}
static void test() {
 Bench b;connect_passive(b);unsigned warm=warm_window(b,6),base=b.ack+warm;b.autoAck=false;
 for(int i=0;i<6;i++)write_data(b,8960,0);
 wait_tx(b,warm+6*8960,"initial TX missing");
 unsigned tA=b.cycles,before=b.readLog.size();
 send(b.ipRx,packet_sack(b.seq,base,{{base+8960,base+4*8960}}));b.run(400);
 check(b.readLog.size()==before,fmt("SACK block honoured without negotiation",b).c_str());
 for(int i=0;i<2;i++){send(b.ipRx,packet(b.seq,base,16));b.run(10);}
 b.run(400);
 check(b.cycles-tA<sweeps(12),"case ran into the RTO window");
 check(b.readLog.size()==before+1,fmt("three duplicate ACKs (one carrying SACK) must fast-retransmit once",b).c_str());
 expect_read(b,before,base,8960,"retransmission read");
 b.autoAck=true;send(b.ipRx,packet(b.seq,base+6*8960,16));b.run(2500);
 check(b.readLog.size()==before+1,fmt("buffer read after the cumulative ACK",b).c_str());
}
int main(){return run_case("sack_not_negotiated",test);}
