#include "../common/csim.hpp"
static std::string fmt(const char *why,const Bench &b){return std::string(why)+" reads="+std::to_string(b.readLog.size())+" closes="+std::to_string(b.closeNotices)+" sid="+std::to_string(b.sid)+" cycle="+std::to_string(b.cycles);}
static void reconnect(Bench &b,unsigned isn,unsigned port) {
 peerPort=port;b.seq=isn+1;b.synack=false;b.fin=false;b.received.clear();b.expectedTx.clear();b.txPayload=0;b.packets.clear();
 send(b.ipRx,packet_syn(isn,true,3));b.run(1000);check(b.synack,"SYN-ACK of the second connection missing");
 send(b.ipRx,packet(b.seq,b.ack,16));b.run(300);b.packets.clear();
}
static void test() {
 Bench b;connect_passive(b,1000,true,true);unsigned sid1=b.sid;
 unsigned warm=warm_window(b,6),base=b.ack+warm;b.autoAck=false;
 for(int i=0;i<6;i++)write_data(b,8960,0);
 wait_tx(b,warm+6*8960,"initial TX missing");
 unsigned t0=b.cycles,before=b.readLog.size();
 wait_read(b,before+1,t0+sweeps(19),"first RTO missing");
 send(b.ipRx,packet(b.seq,base+8960,17));b.seq+=1;b.run(400);
 b.close.write(sid1);b.run(1500);
 check(b.fin,"local FIN missing");
 unsigned closesAfterFirst=b.closeNotices;
 b.run(sweeps(4));
 reconnect(b,50000,40016);
 unsigned before2=b.readLog.size();
 b.autoAck=true;
 for(int k=0;k<30;k++){receive_data(b,700);transmit_data(b,300);}
 check(b.closeNotices==closesAfterFirst,fmt("close notification during the second connection",b).c_str());
 check(b.readLog.size()==before2,fmt("retransmission on the second connection",b).c_str());
}
int main(){return run_case("reconnect_after_loss",test);}
