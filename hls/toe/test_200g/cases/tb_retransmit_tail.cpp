#include "../common/csim.hpp"
static std::string fmt(const char *why,const Bench &b){return std::string(why)+" txPayload="+std::to_string(b.txPayload)+" reads="+std::to_string(b.readLog.size())+" cycle="+std::to_string(b.cycles);}
static void test() {
 Bench b;connect_passive(b);unsigned warm=warm_window(b,6),base=b.ack+warm;b.autoAck=false;
 for(int i=0;i<7;i++)write_data(b,8960);
 b.run(200);check(b.txPayload==warm+7*8960,fmt("initial TX missing",b).c_str());
 unsigned tail=base+6*8960;send(b.ipRx,packet(b.seq,tail,16));
 unsigned tAck=b.cycles,before=b.readLog.size();
 while(b.cycles-tAck<12*MAX_SESSIONS)b.tick();
 check(b.readLog.size()==before,fmt("retransmission before the RTO",b).c_str());
 while(b.readLog.size()<before+1&&b.cycles-tAck<28*MAX_SESSIONS)b.tick();
 check(b.readLog.size()==before+1,fmt("RTO did not resend the lost tail segment",b).c_str());
 check((b.readLog[before].saddr.to_uint()&(BUFFER_SIZE-1))==(tail&(BUFFER_SIZE-1)),"tail retransmission address");
 check(b.readLog[before].bbt==8960,"tail retransmission length");
 b.autoAck=true;send(b.ipRx,packet(b.seq,base+7*8960,16));b.run(2500);
 check(b.readLog.size()==before+1,fmt("buffer read after the full ACK",b).c_str());
 write_data(b,64,500);
 check(b.txPayload==warm+7*8960+64,fmt("session did not survive the recovery",b).c_str());
}
int main(){return run_case("retransmit_tail",test);}
