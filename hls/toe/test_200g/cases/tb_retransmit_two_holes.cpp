// Regression: two losses in one window cost one segment per trigger: the third duplicate ACK resends the
// first hole, the partial ACK that follows resends the second hole at once, and nothing is sent twice.
#include "../common/csim.hpp"
static std::string fmt(const char *why,const Bench &b){return std::string(why)+" txPayload="+std::to_string(b.txPayload)+" reads="+std::to_string(b.readLog.size())+" cycle="+std::to_string(b.cycles);}
static void expect_read(const Bench &b,unsigned index,unsigned seq,const char *why){
 check(b.readLog.size()>index,why);
 check((b.readLog[index].saddr.to_uint()&(BUFFER_SIZE-1))==(seq&(BUFFER_SIZE-1)),why);
 check(b.readLog[index].bbt==8960,why);
}
static void test() {
 Bench b;connect_passive(b);b.autoAck=false;
 for(int i=0;i<7;i++){
  Bytes data(8960);for(unsigned k=0;k<data.size();k++)data[k]=(b.expectedTx.size()+k)*31+9;
  b.expectedTx.insert(b.expectedTx.end(),data.begin(),data.end());
  b.txMeta.write(appTxMeta(b.sid,data.size()));b.run(20);send(b.appTx,data);b.run(150);
 }
 b.run(200);check(b.txPayload==7*8960,fmt("initial TX missing",b).c_str());
 unsigned hole1=b.ack+8960,hole2=b.ack+4*8960,all=b.ack+7*8960;
 send(b.ipRx,packet(b.seq,hole1,16));b.run(30);
 unsigned before=b.readLog.size();
 for(int i=0;i<3;i++){send(b.ipRx,packet(b.seq,hole1,16));b.run(10);}
 b.run(300);
 check(b.readLog.size()==before+1,fmt("first hole must be resent with exactly one read",b).c_str());
 expect_read(b,before,hole1,"first hole read");
 send(b.ipRx,packet(b.seq,hole2,16));b.run(300);
 check(b.readLog.size()==before+2,fmt("partial ACK did not resend the second hole at once",b).c_str());
 expect_read(b,before+1,hole2,"second hole read");
 b.autoAck=true;send(b.ipRx,packet(b.seq,all,16));b.run(2000);
 check(b.readLog.size()==before+2,fmt("buffer read after the full ACK",b).c_str());
 Bytes tail(64);for(unsigned k=0;k<tail.size();k++)tail[k]=(b.expectedTx.size()+k)*31+9;
 b.expectedTx.insert(b.expectedTx.end(),tail.begin(),tail.end());
 b.txMeta.write(appTxMeta(b.sid,tail.size()));b.run(20);send(b.appTx,tail);b.run(500);
 check(b.txPayload==7*8960+64,fmt("session did not survive the recovery",b).c_str());
}
int main(){return run_case("retransmit_two_holes",test);}
