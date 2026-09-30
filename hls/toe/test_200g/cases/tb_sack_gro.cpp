#include "../common/csim.hpp"
static std::string fmt(const char *why,const Bench &b){return std::string(why)+" txPayload="+std::to_string(b.txPayload)+" reads="+std::to_string(b.readLog.size())+" cycle="+std::to_string(b.cycles);}
static void test() {
 Bench b;connect_passive(b,1000,true,true);unsigned warm=warm_window(b,6),base=b.ack+warm;b.autoAck=false;
 for(int i=0;i<6;i++)write_data(b,8960,0);
 wait_tx(b,warm+6*8960,"initial TX missing");
 unsigned tA=b.cycles,before=b.readLog.size();
 send(b.ipRx,packet_sack(b.seq,base,{{base+8960,base+4*8960}}));
 wait_read(b,before+1,tA+sweeps(3),"one SACK block covering three segments must fast-retransmit at once");
 expect_read(b,before,base,8960,"retransmission read");
 b.run(300);
 check(b.cycles-tA<sweeps(12),"case ran into the RTO window");
 check(b.readLog.size()==before+1,fmt("more than one segment retransmitted",b).c_str());
 send(b.ipRx,packet_sack(b.seq,base,{{base+8960,base+4*8960}}));b.run(300);
 check(b.readLog.size()==before+1,fmt("a repeated SACK block resent",b).c_str());
 b.autoAck=true;send(b.ipRx,packet(b.seq,base+6*8960,16));b.run(2500);
 check(b.readLog.size()==before+1,fmt("buffer read after the cumulative ACK",b).c_str());
 write_data(b,64,500);
 check(b.txPayload==warm+6*8960+64,fmt("session did not survive the recovery",b).c_str());
}
int main(){return run_case("sack_gro",test);}
