#include "../common/csim.hpp"
static void test() {
 Bench b;connect_passive(b);receive_data(b,65);unsigned c=b.closeNotices,n=b.table.size();
 b.close.write(b.sid);b.run(500);check(b.fin&&b.finCount==1,"local FIN");b.ack+=1;
 b.run(sweeps(30));
 send(b.ipRx,packet(b.seq,b.ack,17));b.run(1000);unsigned tF=b.cycles;
 check(get(b.packets.back(),28,4)==b.seq+1,"peer FIN not ACKed");
 check(b.closeNotices==c+1,"closed notification for the peer FIN");
 while(b.table.size()==n){check(b.cycles<tF+sweeps(66),"TIME_WAIT release missing");b.tick();}
 check(b.cycles>=tF+sweeps(55),"TIME_WAIT timer not re-armed by the FIN");
 b.run(sweeps(70));check(b.closeNotices==c+1&&b.finCount==1,"release notified or FIN retransmitted");
}
int main(){return run_case("fin_wait2_fin",test);}
