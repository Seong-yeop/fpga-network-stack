#include "../common/csim.hpp"
static std::string fmt(const char *why,const Bench &b){return std::string(why)+" txPayload="+std::to_string(b.txPayload)+" reads="+std::to_string(b.readLog.size())+" cycle="+std::to_string(b.cycles);}
static void expect_read(const Bench &b,unsigned index,unsigned seq,const char *why){
 check(b.readLog.size()>index,why);
 check((b.readLog[index].saddr.to_uint()&(BUFFER_SIZE-1))==(seq&(BUFFER_SIZE-1)),why);
 check(b.readLog[index].bbt==8960,why);
}
static void test() {
 Bench b;connect_passive(b);unsigned warm=warm_window(b,6),base=b.ack+warm;b.autoAck=false;
 for(int i=0;i<7;i++)write_data(b,8960);
 b.run(200);check(b.txPayload==warm+7*8960,fmt("initial TX missing",b).c_str());
 unsigned hole=base+8960;send(b.ipRx,packet(b.seq,hole,16));b.run(30);
 unsigned before=b.readLog.size();
 for(int i=0;i<3;i++){send(b.ipRx,packet(b.seq,hole,16));b.run(10);}
 unsigned waited0=0;
 while(b.readLog.size()<before+1&&waited0<400){b.tick();waited0++;}
 check(b.readLog.size()==before+1,fmt("first fast retransmission",b).c_str());
 expect_read(b,before,hole,"first retransmission read");
 unsigned tRead=b.cycles;
 for(int i=0;i<5;i++){send(b.ipRx,packet(b.seq,hole,16));b.run(10);}
 while(b.cycles-tRead<2*MAX_SESSIONS)b.tick();
 check(b.readLog.size()==before+1,fmt("duplicate ACKs after the fast retransmission must not resend",b).c_str());
 while(b.readLog.size()<before+2&&b.cycles-tRead<8*MAX_SESSIONS)b.tick();
 check(b.readLog.size()==before+2,fmt("fast timer did not repeat the retransmission",b).c_str());
 expect_read(b,before+1,hole,"repeated retransmission read");
 b.autoAck=true;send(b.ipRx,packet(b.seq,base+7*8960,16));b.run(2500);
 check(b.readLog.size()==before+2,fmt("buffer read after the cumulative ACK",b).c_str());
 Bytes tail(64);for(unsigned k=0;k<tail.size();k++)tail[k]=(b.expectedTx.size()+k)*31+9;
 b.expectedTx.insert(b.expectedTx.end(),tail.begin(),tail.end());
 b.txMeta.write(appTxMeta(b.sid,tail.size()));b.run(20);send(b.appTx,tail);b.run(500);
 check(b.txPayload==warm+7*8960+64,fmt("session did not survive the recovery",b).c_str());
}
int main(){return run_case("retransmit_lost",test);}
