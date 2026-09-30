#include "../common/csim.hpp"
static std::string fmt(const char *why,const Bench &b){return std::string(why)+" reads="+std::to_string(b.readLog.size())+" cycle="+std::to_string(b.cycles);}
static void test() {
 Bench b;connect_passive(b);unsigned warm=warm_window(b,6),base=b.ack+warm;b.autoAck=false;
 for(int i=0;i<6;i++)write_data(b,8960,0);
 wait_tx(b,warm+6*8960,"initial TX missing");
 unsigned hole=base+8960;send(b.ipRx,ack_with_window(b,hole,65535));b.run(30);
 unsigned tA=b.cycles,before=b.readLog.size();
 unsigned wins[5]={32768,65535,32768,65535,32768};
 for(int i=0;i<5;i++){send(b.ipRx,ack_with_window(b,hole,wins[i]));b.run(10);}
 b.run(400);
 check(b.cycles-tA<sweeps(12),"case ran into the RTO window");
 check(b.readLog.size()==before,fmt("window updates must not count as duplicate ACKs",b).c_str());
 for(int i=0;i<3;i++){send(b.ipRx,ack_with_window(b,hole,32768));b.run(10);}
 b.run(400);
 check(b.cycles-tA<sweeps(12),"case ran into the RTO window");
 check(b.readLog.size()==before+1,fmt("three true duplicate ACKs must fast-retransmit once",b).c_str());
 expect_read(b,before,hole,8960,"retransmission read address");
 for(int i=0;i<2;i++){send(b.ipRx,ack_with_window(b,hole,32768));b.run(10);}
 b.run(400);
 check(b.readLog.size()==before+1,fmt("further duplicate ACKs must not resend",b).c_str());
 b.autoAck=true;send(b.ipRx,ack_with_window(b,base+6*8960,65535));b.run(2000);
 check(b.readLog.size()==before+1,fmt("buffer read after the cumulative ACK",b).c_str());
}
int main(){return run_case("dupack_window",test);}
