#include "../common/csim.hpp"
static std::string fmt(const char *why,const Bench &b){return std::string(why)+" txPayload="+std::to_string(b.txPayload)+" reads="+std::to_string(b.readLog.size())+" cycle="+std::to_string(b.cycles);}
static void wait_payload(Bench &b,unsigned bytes,const char *why){unsigned limit=b.cycles+4000;while(b.txPayload<bytes){check(b.cycles<limit,fmt(why,b).c_str());b.tick();}}
static void test() {
 Bench b;connect_passive(b);unsigned warm=warm_window(b,6),base=b.ack+warm;unsigned tA=b.cycles;b.autoAck=false;
 for(int i=0;i<6;i++)write_data(b,8960,0);
 unsigned before=b.readLog.size();
 for(unsigned k=2;k<=4;k++){wait_payload(b,warm+k*8960,"window TX missing");send(b.ipRx,packet(b.seq,base,16));}
 unsigned limit=b.cycles+1500,keep=b.cycles;
 while((b.readLog.size()<before+1||b.txPayload<warm+6*8960)&&b.cycles<limit){b.tick();if(b.cycles-keep>=60){send(b.ipRx,packet(b.seq,base,16));keep=b.cycles;}}
 check(b.cycles-tA<sweeps(12),"case ran into the RTO window");
 check(b.txPayload==warm+6*8960,fmt("window TX missing",b).c_str());
 check(b.readLog.size()==before+1,fmt("fast retransmission must read one segment",b).c_str());
 expect_read(b,before,base,8960,"retransmission read");
 for(int i=0;i<3;i++){send(b.ipRx,packet(b.seq,base,16));b.run(10);}
 b.run(200);
 check(b.cycles-tA<sweeps(12),"case ran into the RTO window");
 check(b.readLog.size()==before+1,fmt("further duplicate ACKs must not resend",b).c_str());
 b.autoAck=true;send(b.ipRx,packet(b.seq,base+6*8960,16));b.run(2500);
 check(b.readLog.size()==before+1,fmt("buffer read after the cumulative ACK",b).c_str());
 write_data(b,64,500);
 check(b.txPayload==warm+6*8960+64,fmt("session did not survive the recovery",b).c_str());
}
int main(){return run_case("retransmit_window",test);}
