#include "../common/csim.hpp"
static void test() {
 Bench b;connect_passive(b);receive_data(b,65);unsigned c=b.closeNotices,n=b.table.size();
 b.close.write(b.sid);b.run(500);check(b.fin&&b.finCount==1,"local FIN");
 unsigned t0=b.cycles;b.ack+=1;
 receive_data(b,64);
 check(!(b.packets.back()[33]&0x04),"RST on data in FIN_WAIT_2");
 b.synack=false;send(b.ipRx,packet_syn(7000));b.run(500);check(!b.synack,"SYN-ACK on an open session");
 while(b.table.size()==n){check(b.cycles<t0+sweeps(66),"FIN_WAIT_2 timeout release missing");b.tick();}
 check(b.cycles>=t0+sweeps(55),"FIN_WAIT_2 released too early");
 check(b.closeNotices==c+1&&b.finCount==1,"notification at the FIN_WAIT_2 timeout");
 b.run(sweeps(70));check(b.closeNotices==c+1&&b.table.size()==n-1,"late notification or release");
 reopen(b,8000,40000);receive_data(b,65);check(b.table.size()==n,"reconnect on the same tuple");
}
int main(){return run_case("fin_wait2_timeout",test);}
