#include "../common/csim.hpp"
static std::string fmt(const char *why,const Bench &b){return std::string(why)+" reads="+std::to_string(b.readLog.size())+" cycle="+std::to_string(b.cycles)+" accepted="+std::to_string(b.accepted)+" nospace="+std::to_string(b.noSpace);}
static void test() {
 Bench b;connect_passive(b);unsigned warm=warm_window(b,6),base=b.ack+warm;b.autoAck=false;
 for(int i=0;i<6;i++)write_data(b,8960,0);
 wait_tx(b,warm+6*8960,"initial TX missing");
 unsigned recover=base+6*8960;
 send(b.ipRx,packet(b.seq,base,16));b.run(30);
 unsigned tA=b.cycles,before=b.readLog.size();
 for(int i=0;i<3;i++){send(b.ipRx,packet(b.seq,base,16));b.run(10);}
 b.run(400);
 check(b.cycles-tA<sweeps(12),"case ran into the RTO window");
 check(b.readLog.size()==before+1,fmt("first fast retransmission",b).c_str());
 expect_read(b,before,base,8960,"first hole read");
 send(b.ipRx,packet(b.seq,base+5*8960,16));b.run(400);
 check(b.readLog.size()==before+2,fmt("partial ACK must resend the last hole",b).c_str());
 expect_read(b,before+1,base+5*8960,8960,"last hole read");
 send(b.ipRx,packet(b.seq,recover,16));b.run(400);
 check(b.readLog.size()==before+2,fmt("read after the recovery exit",b).c_str());
 unsigned tB=b.cycles;
 for(int i=0;i<2;i++)write_data(b,8960,0);
 wait_tx(b,warm+8*8960,"segments after recovery missing");
 b.allowNoSpace=true;check(!try_write(b,tx_pattern(b,8960)),fmt("a third segment must not fit the exit window",b).c_str());
 for(int i=0;i<4;i++){send(b.ipRx,packet(b.seq,recover,16));b.run(10);}
 b.run(400);
 check(b.cycles-tB<sweeps(12),"case ran into the RTO window");
 check(b.readLog.size()==before+2,fmt("duplicate ACKs at recover must not fast-retransmit again",b).c_str());
 send(b.ipRx,packet(b.seq,recover+8960,16));b.run(30);
 unsigned tC=b.cycles;
 for(int i=0;i<3;i++){send(b.ipRx,packet(b.seq,recover+8960,16));b.run(10);}
 b.run(400);
 check(b.cycles-tC<sweeps(12),"case ran into the RTO window");
 check(b.readLog.size()==before+3,fmt("duplicate ACKs beyond recover must fast-retransmit",b).c_str());
 expect_read(b,before+2,recover+8960,8960,"retransmission beyond recover");
 b.autoAck=true;send(b.ipRx,packet(b.seq,recover+2*8960,16));b.run(2000);
 check(b.readLog.size()==before+3,fmt("buffer read after the final ACK",b).c_str());
}
int main(){return run_case("recover_gate",test);}
