#include "../common/csim.hpp"
static std::string fmt(const char *why,const Bench &b){return std::string(why)+" reads="+std::to_string(b.readLog.size())+" cycle="+std::to_string(b.cycles);}
static void test() {
 Bench b;connect_passive(b,1000,true,true);unsigned warm=warm_window(b,6),base=b.ack+warm;b.autoAck=false;
 for(int i=0;i<6;i++)write_data(b,8960,0);
 wait_tx(b,warm+6*8960,"initial TX missing");
 unsigned tA=b.cycles,before=b.readLog.size();
 Bytes data(72);for(unsigned i=0;i<data.size();i++)data[i]=(b.received.size()+i)*17+5;
 Bytes expected=b.received;expected.insert(expected.end(),data.begin(),data.end());
 send(b.ipRx,packet_sack(b.seq,base,{{base+8960,base+4*8960}},data));b.seq+=data.size();
 wait_read(b,before+1,tA+sweeps(3),"a data segment carrying a new SACK block must count as a duplicate ACK");
 expect_read(b,before,base,8960,"retransmission read");
 b.run(300);check(b.received==expected,"RX bytes differ");
 check(b.readLog.size()==before+1,fmt("more than one segment retransmitted",b).c_str());
 b.autoAck=true;send(b.ipRx,packet(b.seq,base+6*8960,16));b.run(2500);
 check(b.readLog.size()==before+1,fmt("buffer read after the cumulative ACK",b).c_str());
}
int main(){return run_case("sack_with_data",test);}
