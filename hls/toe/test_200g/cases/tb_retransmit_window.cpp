#include "../common/csim.hpp"
static std::string fmt(const char *why,const Bench &b){return std::string(why)+" txPayload="+std::to_string(b.txPayload)+" reads="+std::to_string(b.readLog.size())+" cycle="+std::to_string(b.cycles);}
static void wait_payload(Bench &b,unsigned bytes,const char *why){unsigned limit=b.cycles+4000;while(b.txPayload<bytes){check(b.cycles<limit,fmt(why,b).c_str());b.tick();}}
static void test() {
 Bench b;connect_passive(b);unsigned warm=warm_window(b,6),base=b.ack+warm;b.autoAck=false;
 for(int i=0;i<6;i++)write_data(b,8960,0);
 unsigned before=b.readLog.size();
 for(unsigned k=2;k<=4;k++){wait_payload(b,warm+k*8960,"window TX missing");send(b.ipRx,packet(b.seq,base,16));}
 unsigned limit=b.cycles+1500,keep=b.cycles;
 while((b.readLog.size()<before+4||b.txPayload<warm+6*8960)&&b.cycles<limit){b.tick();if(b.cycles-keep>=60){send(b.ipRx,packet(b.seq,base,16));keep=b.cycles;}}
 check(b.txPayload==warm+6*8960,fmt("window TX missing",b).c_str());
 check(b.readLog.size()==before+4,fmt("original fast retransmission must read four segments",b).c_str());
 for(unsigned i=0;i<4;i++){
  check((b.readLog[before+i].saddr.to_uint()&(BUFFER_SIZE-1))==((base+i*8960)&(BUFFER_SIZE-1)),"retransmission read address");
  check(b.readLog[before+i].bbt==8960,"retransmission segment length");
 }
 b.autoAck=true;send(b.ipRx,packet(b.seq,base+6*8960,16));b.run(2500);
 check(b.readLog.size()==before+4,fmt("buffer read after the cumulative ACK",b).c_str());
 Bytes tail(64);for(unsigned k=0;k<tail.size();k++)tail[k]=(b.expectedTx.size()+k)*31+9;
 b.expectedTx.insert(b.expectedTx.end(),tail.begin(),tail.end());
 b.txMeta.write(appTxMeta(b.sid,tail.size()));b.run(20);send(b.appTx,tail);b.run(500);
 check(b.txPayload==warm+6*8960+64,fmt("session did not survive the recovery",b).c_str());
}
int main(){return run_case("retransmit_window",test);}
