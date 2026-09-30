#include "../common/csim.hpp"
static std::string fmt(const char *why,const Bench &b){return std::string(why)+" reads="+std::to_string(b.readLog.size())+" cycle="+std::to_string(b.cycles);}
static void test() {
 Bench b;connect_passive(b,1000,true,true);unsigned warm=warm_window(b,6),base=b.ack+warm;b.autoAck=false;
 unsigned t0=b.cycles,before=b.readLog.size();
 for(int i=0;i<6;i++)write_data(b,8960,0);
 wait_tx(b,warm+6*8960,"initial TX missing");
 send(b.ipRx,packet_sack(b.seq,base,{{base+8960,base+2*8960}}));b.run(200);
 no_read_until(b,before,t0+sweeps(15),"one SACKed MSS triggered a retransmission before the RTO");
 wait_read(b,before+1,t0+sweeps(19),"RTO missing");
 expect_read(b,before,base,8960,"RTO retransmission");
 send(b.ipRx,packet_sack(b.seq,base,{{base+8960,base+4*8960}}));b.run(400);
 check(b.readLog.size()==before+1,fmt("SACK information during loss recovery started a fast retransmit",b).c_str());
 send(b.ipRx,packet(b.seq,base+8960,16));b.run(400);
 check(b.readLog.size()==before+2,fmt("advancing ACK in loss recovery must resend the next segment",b).c_str());
 expect_read(b,before+1,base+8960,8960,"second hole read");
 send(b.ipRx,packet(b.seq,base+4*8960,16));b.run(400);
 check(b.readLog.size()==before+3,fmt("advancing ACK past the SACKed range must resend the next segment",b).c_str());
 expect_read(b,before+2,base+4*8960,8960,"third hole read");
 send(b.ipRx,packet(b.seq,base+5*8960,16));b.run(400);
 check(b.readLog.size()==before+4,fmt("last hole",b).c_str());
 expect_read(b,before+3,base+5*8960,8960,"last hole read");
 b.autoAck=true;send(b.ipRx,packet(b.seq,base+6*8960,16));b.run(2500);
 check(b.readLog.size()==before+4,fmt("buffer read after the cumulative ACK",b).c_str());
 write_data(b,64,500);
 check(b.txPayload==warm+6*8960+64,fmt("session did not survive the recovery",b).c_str());
}
int main(){return run_case("sack_rto_clear",test);}
