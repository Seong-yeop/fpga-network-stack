#include "../common/csim.hpp"
static std::string fmt(const char *why,const Bench &b){return std::string(why)+" txPayload="+std::to_string(b.txPayload)+" reads="+std::to_string(b.readLog.size())+" cycle="+std::to_string(b.cycles);}
static void test() {
 Bench b;connect_passive(b);unsigned warm=warm_window(b,6),base=b.ack+warm;b.autoAck=false;
 unsigned t0=b.cycles;
 for(int i=0;i<6;i++)write_data(b,8960,0);
 wait_tx(b,warm+6*8960,"initial TX missing");
 unsigned before=b.readLog.size();
 while(b.cycles-t0<sweeps(6)){b.tick();check(b.readLog.size()==before,fmt("retransmission before any ACK",b).c_str());}
 unsigned tail=base+5*8960;send(b.ipRx,packet(b.seq,tail,16));
 unsigned tA=b.cycles;
 no_read_until(b,before,tA+sweeps(15),"advancing ACK did not restart the RTO");
 wait_read(b,before+1,tA+sweeps(19),"RTO did not resend the lost tail segment");
 expect_read(b,before,tail,8960,"tail retransmission");
 b.autoAck=true;send(b.ipRx,packet(b.seq,base+6*8960,16));b.run(2500);
 check(b.readLog.size()==before+1,fmt("buffer read after the full ACK",b).c_str());
 write_data(b,64,500);
 check(b.txPayload==warm+6*8960+64,fmt("session did not survive the recovery",b).c_str());
}
int main(){return run_case("retransmit_tail",test);}
