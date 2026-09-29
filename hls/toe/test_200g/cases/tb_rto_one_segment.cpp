#include "../common/csim.hpp"
static std::string fmt(const char *why,const Bench &b){return std::string(why)+" reads="+std::to_string(b.readLog.size())+" cycle="+std::to_string(b.cycles)+" accepted="+std::to_string(b.accepted)+" nospace="+std::to_string(b.noSpace);}
static void expect_read(const Bench &b,unsigned index,unsigned seq,const char *why){
 check(b.readLog.size()>index,why);
 check((b.readLog[index].saddr.to_uint()&(BUFFER_SIZE-1))==(seq&(BUFFER_SIZE-1)),why);
 check(b.readLog[index].bbt==8960,why);
}
static void test() {
 Bench b;connect_passive(b);unsigned warm=warm_window(b,6),base=b.ack+warm;b.autoAck=false;
 unsigned t0=b.cycles,before=b.readLog.size();
 for(int i=0;i<7;i++)write_data(b,8960);
 b.run(200);check(b.txPayload==warm+7*8960,fmt("initial TX missing",b).c_str());
 while(b.cycles-t0<12*MAX_SESSIONS)b.tick();
 check(b.readLog.size()==before,fmt("retransmission before the RTO",b).c_str());
 while(b.readLog.size()<before+1&&b.cycles-t0<28*MAX_SESSIONS)b.tick();
 check(b.readLog.size()==before+1,fmt("first RTO retransmission missing",b).c_str());
 expect_read(b,before,base,"RTO must resend the oldest segment only");
 unsigned t1=b.cycles;
 while(b.cycles-t1<24*MAX_SESSIONS)b.tick();
 check(b.readLog.size()==before+1,fmt("second retransmission came before the doubled RTO",b).c_str());
 while(b.readLog.size()<before+2&&b.cycles-t1<48*MAX_SESSIONS)b.tick();
 check(b.readLog.size()==before+2,fmt("doubled RTO did not resend",b).c_str());
 expect_read(b,before+1,base,"second RTO resend address");
 send(b.ipRx,packet(b.seq,base+8960,16));b.run(400);
 check(b.readLog.size()==before+3,fmt("partial ACK must resend the next segment at once",b).c_str());
 expect_read(b,before+2,base+8960,"second hole read");
 send(b.ipRx,packet(b.seq,base+2*8960,16));b.run(400);
 check(b.readLog.size()==before+4,fmt("second partial ACK must resend the next segment",b).c_str());
 expect_read(b,before+3,base+2*8960,"third hole read");
 send(b.ipRx,packet(b.seq,base+7*8960,16));b.run(2500);
 check(b.readLog.size()==before+4,fmt("buffer read after the cumulative ACK",b).c_str());
 b.allowNoSpace=true;
 Bytes d1=tx_pattern(b,8960);b.expectedTx.insert(b.expectedTx.end(),d1.begin(),d1.end());
 check(try_write(b,d1),fmt("one segment must fit the collapsed window",b).c_str());
 Bytes d2=tx_pattern(b,8960);b.expectedTx.insert(b.expectedTx.end(),d2.begin(),d2.end());
 check(!try_write(b,d2),fmt("a second segment must not fit the collapsed window",b).c_str());
 b.run(300);check(b.txPayload==warm+8*8960,fmt("first segment after recovery",b).c_str());
 send(b.ipRx,packet(b.seq,base+8*8960,16));b.run(200);
 check(try_write(b,d2),fmt("the ACK must open the window again",b).c_str());
 b.autoAck=true;b.run(600);check(b.txPayload==warm+9*8960,fmt("second segment after recovery",b).c_str());
}
int main(){return run_case("rto_one_segment",test);}
