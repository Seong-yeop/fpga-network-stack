#include "../common/csim.hpp"
static void test() {
 Bench b;connect_passive(b);receive_data(b,65);unsigned c=b.closeNotices,n=b.table.size();
 b.finAck=false;
 b.close.write(b.sid);b.run(500);check(b.fin&&b.finCount==1,"local FIN");
 unsigned t0=b.cycles,finSeq=get(b.packets.back(),24,4),fins=1;
 while(b.table.size()==n){
  check(b.cycles<t0+sweeps(360),"release after the FIN retransmissions missing");b.tick();
  if(b.finCount!=fins){fins=b.finCount;check(get(b.packets.back(),24,4)==finSeq,"retransmitted FIN sequence");}
 }
 check(b.finCount==7,"FIN count before the give-up");
 check(b.cycles>=t0+sweeps(340),"give-up too early");
 check(b.closeNotices==c+1,"notification at the give-up");
 b.run(sweeps(70));check(b.closeNotices==c+1&&b.finCount==7,"activity after the give-up");
}
int main(){return run_case("fin_rto_exhaust",test);}
