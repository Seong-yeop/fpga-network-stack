#include "../common/csim.hpp"
static std::string fmt(const char *why,const Bench &b){return std::string(why)+" reads="+std::to_string(b.readLog.size())+" cycle="+std::to_string(b.cycles);}
static Bytes payload(const Bench &b,unsigned n,unsigned salt){Bytes d(n);for(unsigned i=0;i<n;i++)d[i]=(b.received.size()+i)*13+salt;return d;}
static void test() {
 Bench b;connect_passive(b,1000,true,true);unsigned warm=warm_window(b,6),base=b.ack+warm;b.autoAck=false;
 for(int i=0;i<6;i++)write_data(b,8960,0);
 wait_tx(b,warm+6*8960,"initial TX missing");
 unsigned before=b.readLog.size();
 Bytes expected=b.received;
 Bytes d1=payload(b,300,7);expected.insert(expected.end(),d1.begin(),d1.end());
 send(b.ipRx,packet_sack(b.seq,base,{{base+8960,base+2*8960}},d1));b.seq+=d1.size();
 unsigned t1=b.cycles;
 no_read_until(b,before,t1+sweeps(2),"one SACKed MSS on a multi-word data segment triggered a retransmission");
 unsigned tA=b.cycles;
 send(b.ipRx,packet_sack(b.seq,base,{{base+8960,base+4*8960}}));
 wait_read(b,before+1,tA+sweeps(3),"a pure ACK SACKing 3 MSS after a multi-word SACK data segment must enter fast retransmit");
 expect_read(b,before,base,8960,"fast retransmission read");
 unsigned end=base+6*8960;
 send(b.ipRx,packet(b.seq,end,16));b.run(300);
 check(b.readLog.size()==before+1,fmt("more than one segment retransmitted",b).c_str());
 for(int k=0;k<3;k++){
  Bytes d=payload(b,300,11+k);expected.insert(expected.end(),d.begin(),d.end());
  send(b.ipRx,packet_sack(b.seq,end,{{end+8960,end+2*8960}},d));b.seq+=d.size();b.run(100);
 }
 Bytes tail=payload(b,64,29);expected.insert(expected.end(),tail.begin(),tail.end());
 send(b.ipRx,packet(b.seq,end,16,tail));b.seq+=tail.size();
 b.run(1500);
 check(b.received==expected,"RX bytes differ after multi-word SACK data segments");
 b.autoAck=true;b.run(2500);
 check(b.readLog.size()==before+1,fmt("buffer read after the cumulative ACK",b).c_str());
 write_data(b,64,500);
}
int main(){return run_case("sack_multiword",test);}
