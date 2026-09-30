#include "../common/csim.hpp"
static std::string fmt(const char *why,const Bench &b){return std::string(why)+" txPayload="+std::to_string(b.txPayload)+" reads="+std::to_string(b.readLog.size())+" cycle="+std::to_string(b.cycles);}
static void test() {
 Bench b;connect_passive(b);unsigned warm=warm_window(b,6),base=b.ack+warm;b.autoAck=false;
 for(int i=0;i<6;i++)write_data(b,8960,0);
 wait_tx(b,warm+6*8960,"initial TX missing");
 unsigned hole1=base+8960,hole2=base+4*8960,all=base+6*8960;
 send(b.ipRx,packet(b.seq,hole1,16));b.run(30);
 unsigned tA=b.cycles,before=b.readLog.size();
 for(int i=0;i<3;i++){send(b.ipRx,packet(b.seq,hole1,16));b.run(10);}
 b.run(300);
 check(b.cycles-tA<sweeps(12),"case ran into the RTO window");
 check(b.readLog.size()==before+1,fmt("first hole must be resent with exactly one read",b).c_str());
 expect_read(b,before,hole1,8960,"first hole read");
 send(b.ipRx,packet(b.seq,hole2,16));b.run(300);
 check(b.cycles-tA<sweeps(12),"case ran into the RTO window");
 check(b.readLog.size()==before+2,fmt("partial ACK did not resend the second hole at once",b).c_str());
 expect_read(b,before+1,hole2,8960,"second hole read");
 b.autoAck=true;send(b.ipRx,packet(b.seq,all,16));b.run(2000);
 check(b.readLog.size()==before+2,fmt("buffer read after the full ACK",b).c_str());
 write_data(b,64,500);
 check(b.txPayload==warm+6*8960+64,fmt("session did not survive the recovery",b).c_str());
}
int main(){return run_case("retransmit_two_holes",test);}
