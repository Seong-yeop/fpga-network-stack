#include "../common/csim.hpp"
static std::string fmt(const char *why,const Bench &b){return std::string(why)+" reads="+std::to_string(b.readLog.size())+" cycle="+std::to_string(b.cycles);}
static void test() {
 Bench b;connect_passive(b,1000,true,true);unsigned warm=warm_window(b,6),base=b.ack+warm;b.autoAck=false;
 for(int i=0;i<6;i++)write_data(b,8960,0);
 wait_tx(b,warm+6*8960,"initial TX missing");
 unsigned tA=b.cycles,before=b.readLog.size();
 Bytes data(64);for(unsigned i=0;i<data.size();i++)data[i]=(b.received.size()+i)*17+5;
 for(int i=0;i<3;i++){
  Bytes expected=b.received;expected.insert(expected.end(),data.begin(),data.end());
  send(b.ipRx,packet_sack(b.seq,base,{{base-8960,base}},data));b.seq+=data.size();b.run(200);
  check(b.received==expected,"RX bytes differ");
  for(unsigned k=0;k<data.size();k++)data[k]=(b.received.size()+k)*17+5;
 }
 b.run(400);
 check(b.cycles-tA<sweeps(12),"case ran into the RTO window");
 check(b.readLog.size()==before,fmt("D-SACK blocks below the ACK counted as new SACK data",b).c_str());
 send(b.ipRx,packet_sack(b.seq,base,{{base+8960,base+2*8960},{base+8960,base+4*8960}}));b.run(200);
 send(b.ipRx,packet_sack(b.seq,base,{{base+8960,base+2*8960},{base+8960,base+4*8960}}));b.run(200);
 check(b.readLog.size()==before+1,fmt("a first block inside the second must count once as new data",b).c_str());
 expect_read(b,before,base,8960,"retransmission read");
 b.autoAck=true;send(b.ipRx,packet(b.seq,base+6*8960,16));b.run(2500);
 check(b.readLog.size()==before+1,fmt("buffer read after the cumulative ACK",b).c_str());
}
int main(){return run_case("sack_dsack",test);}
