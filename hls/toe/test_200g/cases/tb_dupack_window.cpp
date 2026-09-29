#include "../common/csim.hpp"
static std::string fmt(const char *why,const Bench &b){return std::string(why)+" reads="+std::to_string(b.readLog.size())+" cycle="+std::to_string(b.cycles);}
static Bytes ack_with_window(const Bench &b,unsigned ack,unsigned win){Bytes p=packet(b.seq,ack,16);put(p,34,win);return resign(p);}
static void test() {
 Bench b;connect_passive(b);unsigned warm=warm_window(b,6),base=b.ack+warm;b.autoAck=false;
 for(int i=0;i<7;i++)write_data(b,8960);
 b.run(200);check(b.txPayload==warm+7*8960,fmt("initial TX missing",b).c_str());
 unsigned hole=base+8960;send(b.ipRx,packet(b.seq,hole,16));b.run(30);
 unsigned before=b.readLog.size();
 unsigned wins[6]={32768,65535,32768,65535,32768,65535};
 for(int i=0;i<6;i++){send(b.ipRx,ack_with_window(b,hole,wins[i]));b.run(10);}
 b.run(400);
 check(b.readLog.size()==before,fmt("window updates must not count as duplicate ACKs",b).c_str());
 for(int i=0;i<3;i++){send(b.ipRx,ack_with_window(b,hole,65535));b.run(10);}
 b.run(400);
 check(b.readLog.size()==before+1,fmt("three true duplicate ACKs must fast-retransmit once",b).c_str());
 check((b.readLog[before].saddr.to_uint()&(BUFFER_SIZE-1))==(hole&(BUFFER_SIZE-1)),"retransmission read address");
 b.autoAck=true;send(b.ipRx,packet(b.seq,base+7*8960,16));b.run(2000);
 check(b.readLog.size()==before+1,fmt("buffer read after the cumulative ACK",b).c_str());
}
int main(){return run_case("dupack_window",test);}
