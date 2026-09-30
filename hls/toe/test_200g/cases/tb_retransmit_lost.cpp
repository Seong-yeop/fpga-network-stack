#include "../common/csim.hpp"
static std::string fmt(const char *why,const Bench &b){return std::string(why)+" txPayload="+std::to_string(b.txPayload)+" reads="+std::to_string(b.readLog.size())+" cycle="+std::to_string(b.cycles);}
static void test() {
 Bench b;connect_passive(b);unsigned warm=warm_window(b,6),base=b.ack+warm;b.autoAck=false;
 for(int i=0;i<6;i++)write_data(b,8960,0);
 wait_tx(b,warm+6*8960,"initial TX missing");
 unsigned hole=base+8960;send(b.ipRx,packet(b.seq,hole,16));b.run(30);
 unsigned tA=b.cycles,before=b.readLog.size();
 for(int i=0;i<3;i++){send(b.ipRx,packet(b.seq,hole,16));b.run(10);}
 wait_read(b,before+1,tA+sweeps(3),"first fast retransmission");
 expect_read(b,before,hole,8960,"first retransmission read");
 unsigned keep=b.cycles;
 while(b.cycles-tA<sweeps(12)){b.tick();if(b.cycles-keep>=60){send(b.ipRx,packet(b.seq,hole,16));keep=b.cycles;}check(b.readLog.size()==before+1,fmt("duplicate ACKs after the fast retransmission must not resend",b).c_str());}
 no_read_until(b,before+1,tA+sweeps(15),"duplicate ACKs restarted the RTO");
 wait_read(b,before+2,tA+sweeps(19),"RTO did not repeat the lost retransmission");
 expect_read(b,before+1,hole,8960,"repeated retransmission read");
 b.autoAck=true;send(b.ipRx,packet(b.seq,base+6*8960,16));b.run(2500);
 check(b.readLog.size()==before+2,fmt("buffer read after the cumulative ACK",b).c_str());
 write_data(b,64,500);
 check(b.txPayload==warm+6*8960+64,fmt("session did not survive the recovery",b).c_str());
}
int main(){return run_case("retransmit_lost",test);}
