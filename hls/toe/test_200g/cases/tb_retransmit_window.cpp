// Regression: one retransmission event covers the whole unacked window instead of stopping after four
// segments, so a single loss in a stream of seven segments is repaired by one event.
#include "../common/csim.hpp"
static std::string fmt(const char *why,const Bench &b){return std::string(why)+" txPayload="+std::to_string(b.txPayload)+" reads="+std::to_string(b.readLog.size())+" cycle="+std::to_string(b.cycles);}
static void test() {
 Bench b;connect_passive(b);b.autoAck=false;
 for(int i=0;i<7;i++){
  Bytes data(8960);for(unsigned k=0;k<data.size();k++)data[k]=(b.expectedTx.size()+k)*31+9;
  b.expectedTx.insert(b.expectedTx.end(),data.begin(),data.end());
  b.txMeta.write(appTxMeta(b.sid,data.size()));b.run(20);send(b.appTx,data);b.run(150);
 }
 b.run(200);check(b.txPayload==7*8960,fmt("initial TX missing",b).c_str());
 unsigned acked=b.ack+8960;send(b.ipRx,packet(b.seq,acked,16));b.run(30);
 unsigned before=b.readLog.size();
 for(int i=0;i<3;i++){send(b.ipRx,packet(b.seq,acked,16));b.run(10);}
 b.run(300);
 check(b.readLog.size()>=before+6,fmt("retransmission issued fewer than six reads",b).c_str());
 unsigned base=b.readLog[before].saddr.to_uint();
 for(unsigned k=0;k<6;k++){
  check(((b.readLog[before+k].saddr.to_uint()-base)&(BUFFER_SIZE-1))==k*8960,"retransmission reads are not one contiguous window");
  check(b.readLog[before+k].bbt==8960,"retransmission segment length");
 }
 b.autoAck=true;send(b.ipRx,packet(b.seq,acked,16));b.run(2000);
 Bytes tail(64);for(unsigned k=0;k<tail.size();k++)tail[k]=(b.expectedTx.size()+k)*31+9;
 b.expectedTx.insert(b.expectedTx.end(),tail.begin(),tail.end());
 b.txMeta.write(appTxMeta(b.sid,tail.size()));b.run(20);send(b.appTx,tail);b.run(500);
 check(b.txPayload==7*8960+64,fmt("session did not survive the recovery",b).c_str());
}
int main(){return run_case("retransmit_window",test);}
