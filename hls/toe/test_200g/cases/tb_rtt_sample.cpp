#include "../common/csim.hpp"
static std::string fmt(const char *why,const Bench &b){return std::string(why)+" reads="+std::to_string(b.readLog.size())+" cycle="+std::to_string(b.cycles);}
static void ack_after(Bench &b,unsigned seq,unsigned wait){b.run(wait);send(b.ipRx,packet(b.seq,seq,16));b.run(100);}
static unsigned rto_wait(Bench &b,unsigned limit){unsigned before=b.readLog.size(),t0=b.cycles;while(b.readLog.size()==before&&b.cycles-t0<limit)b.tick();check(b.readLog.size()==before+1,fmt("RTO retransmission missing",b).c_str());return b.cycles-t0;}
static void test() {
 Bench b;connect_passive(b);b.autoAck=false;
 unsigned seq=b.ack;
 for(int i=0;i<2;i++){write_data(b,8960,50);seq+=8960;ack_after(b,seq,3000);}
 write_data(b,8960,50);seq+=8960;
 unsigned waited=rto_wait(b,120*MAX_SESSIONS);
 check(waited>40*MAX_SESSIONS,("RTO ignored the measured round trips: "+std::to_string(waited)).c_str());
 check(waited<90*MAX_SESSIONS,("RTO too long for the measured round trips: "+std::to_string(waited)).c_str());
 ack_after(b,seq,20);
 for(int i=0;i<3;i++){write_data(b,8960,50);seq+=8960;ack_after(b,seq,20);}
 write_data(b,8960,50);seq+=8960;
 waited=rto_wait(b,60*MAX_SESSIONS);
 check(waited<30*MAX_SESSIONS,("RTO did not fall back to the floor after fresh samples: "+std::to_string(waited)).c_str());
 b.autoAck=true;send(b.ipRx,packet(b.seq,seq,16));b.run(500);
 write_data(b,64,500);check(b.txPayload==b.expectedTx.size(),fmt("session did not survive",b).c_str());
}
int main(){return run_case("rtt_sample",test);}
