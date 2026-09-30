#include "../common/csim.hpp"
static std::string fmt(const char *why,const Bench &b){return std::string(why)+" reads="+std::to_string(b.readLog.size())+" cycle="+std::to_string(b.cycles)+" accepted="+std::to_string(b.accepted)+" nospace="+std::to_string(b.noSpace);}
static void test() {
 Bench b;connect_passive(b);unsigned warm=warm_window(b,6),base=b.ack+warm;b.autoAck=false;
 unsigned t0=b.cycles,before=b.readLog.size();
 for(int i=0;i<6;i++)write_data(b,8960,0);
 wait_tx(b,warm+6*8960,"initial TX missing");
 check(b.cycles-t0<sweeps(4),"initial writes took too long");
 no_read_until(b,before,t0+sweeps(15),"retransmission before the RTO");
 wait_read(b,before+1,t0+sweeps(19),"first RTO retransmission missing");
 expect_read(b,before,base,8960,"RTO must resend the oldest segment only");
 unsigned t1=b.cycles;
 b.run(50);b.allowNoSpace=true;
 check(!try_write(b,tx_pattern(b,8960)),fmt("the loss window must reach the application",b).c_str());
 no_read_until(b,before+1,t1+sweeps(31),"second retransmission came before the doubled RTO");
 wait_read(b,before+2,t1+sweeps(34),"doubled RTO did not resend");
 expect_read(b,before+1,base,8960,"second RTO resend address");
 send(b.ipRx,packet(b.seq,base+8960,16));b.run(400);
 check(b.readLog.size()==before+3,fmt("partial ACK in loss recovery must resend the next segment at once",b).c_str());
 expect_read(b,before+2,base+8960,8960,"second hole read");
 check(!try_write(b,tx_pattern(b,8960)),fmt("a full segment must not fit while the window slow-starts",b).c_str());
 send(b.ipRx,packet(b.seq,base+2*8960,16));b.run(400);
 check(b.readLog.size()==before+4,fmt("second partial ACK must resend the next segment",b).c_str());
 expect_read(b,before+3,base+2*8960,8960,"third hole read");
 send(b.ipRx,packet(b.seq,base+3*8960,16));b.run(400);
 check(b.readLog.size()==before+5,fmt("third partial ACK must resend the next segment",b).c_str());
 expect_read(b,before+4,base+3*8960,8960,"fourth hole read");
 write_data(b,64,0);b.run(200);
 check(b.txPayload==warm+6*8960+64,fmt("small segment after the partial ACKs",b).c_str());
 check(b.readLog.size()==before+5,fmt("read while sending new data",b).c_str());
 send(b.ipRx,packet(b.seq,base+6*8960,16));
 unsigned tA=b.cycles;b.run(400);
 check(b.readLog.size()==before+5,fmt("read at the loss recovery exit",b).c_str());
 no_read_until(b,before+5,tA+sweeps(59),"restart without collapse lost the backoff");
 wait_read(b,before+6,tA+sweeps(62),"backed-off RTO did not resend the tail");
 expect_read(b,before+5,base+6*8960,64,"tail resend after the backed-off RTO");
 write_data(b,64,0);write_data(b,64,0);b.run(200);
 check(b.txPayload==warm+6*8960+192,fmt("segments after the backed-off RTO",b).c_str());
 check(b.readLog.size()==before+6,fmt("read while sending new data after the RTO",b).c_str());
 send(b.ipRx,packet(b.seq,base+6*8960+128,16));
 unsigned tB=b.cycles;b.run(400);
 check(b.readLog.size()==before+6,fmt("read at the ACK beyond recover",b).c_str());
 no_read_until(b,before+6,tB+sweeps(15),"collapse produced an early RTO");
 wait_read(b,before+7,tB+sweeps(19),"collapsed RTO did not resend the last segment");
 expect_read(b,before+6,base+6*8960+128,64,"resend after the collapsed RTO");
 b.autoAck=true;send(b.ipRx,packet(b.seq,base+6*8960+192,16));b.run(2500);
 check(b.readLog.size()==before+7,fmt("buffer read after the final ACK",b).c_str());
 write_data(b,64,500);
 check(b.txPayload==warm+6*8960+256,fmt("session did not survive the recovery",b).c_str());
}
int main(){return run_case("rto_one_segment",test);}
