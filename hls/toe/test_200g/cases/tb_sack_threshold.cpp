#include "../common/csim.hpp"
static std::string fmt(const char *why,const Bench &b){return std::string(why)+" reads="+std::to_string(b.readLog.size())+" cycle="+std::to_string(b.cycles);}
static unsigned hole(Bench &b) {
 b.autoAck=false;unsigned base=b.ack+b.expectedTx.size();
 for(int i=0;i<6;i++)write_data(b,8960,0);
 wait_tx(b,b.expectedTx.size(),"segments missing");
 return base;
}
static void test() {
 Bench b;connect_passive(b,1000,true,true);warm_window(b,6);
 unsigned base=hole(b),before=b.readLog.size(),tA=b.cycles;
 send(b.ipRx,packet_sack(b.seq,base,{{base+8960,base+2*8960}}));b.run(400);
 check(b.readLog.size()==before,fmt("one SACKed MSS must not trigger",b).c_str());
 send(b.ipRx,packet_sack(b.seq,base,{{base+8960,base+3*8960}}));b.run(400);
 check(b.cycles-tA<sweeps(12),"case ran into the RTO window");
 check(b.readLog.size()==before,fmt("exactly two SACKed MSS must not trigger",b).c_str());
 b.autoAck=true;send(b.ipRx,packet(b.seq,base+6*8960,16));b.run(2500);
 check(b.readLog.size()==before,fmt("read after the cumulative ACK",b).c_str());
 base=hole(b);before=b.readLog.size();tA=b.cycles;
 send(b.ipRx,packet_sack(b.seq,base,{{base+8960,base+3*8960+1}}));
 wait_read(b,before+1,tA+sweeps(3),"two SACKed MSS plus one byte must trigger");
 expect_read(b,before,base,8960,"retransmission read");
 b.autoAck=true;send(b.ipRx,packet(b.seq,base+6*8960,16));b.run(2500);
 check(b.readLog.size()==before+1,fmt("read after the cumulative ACK",b).c_str());
 write_data(b,64,500);
 check(b.txPayload==b.expectedTx.size(),fmt("session did not survive the recovery",b).c_str());
}
int main(){return run_case("sack_threshold",test);}
