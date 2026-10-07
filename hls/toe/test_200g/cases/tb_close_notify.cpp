#include "../common/csim.hpp"
static void fin_wait_session(Bench &b,unsigned isn,unsigned port,const char *why) {
 reopen(b,isn,port);receive_data(b,65);b.close.write(b.sid);b.run(500);check(b.fin&&b.finCount==1,why);b.finCount=0;b.ack+=1;
}
static void test() {
 Bench b;connect_passive(b);receive_data(b,65);
 unsigned c=b.closeNotices;
 b.close.write(b.sid);b.run(500);check(b.fin&&b.finCount==1&&b.closeNotices==c,"S1 local FIN");b.finCount=0;b.ack+=1;
 send(b.ipRx,packet(b.seq,b.ack,17));b.run(1000);
 check(get(b.packets.back(),28,4)==b.seq+1,"S1 peer FIN not ACKed");
 check(b.closeNotices==c+1,"S1 closed notification for the peer FIN after our FIN");
 send(b.ipRx,packet(b.seq,b.ack,17));b.run(1000);
 check(b.closeNotices==c+1,"S1 retransmitted FIN notified again");
 unsigned t=b.cycles;
 while(!b.table.empty()){check(b.cycles<t+sweeps(66),"S1 TIME_WAIT release missing");b.tick();}
 check(b.cycles>=t+sweeps(45)&&b.closeNotices==c+1,"S1 TIME_WAIT release timing or notification");

 fin_wait_session(b,50000,40001,"S2 local FIN");c=b.closeNotices;
 Bytes data(129,0x6a),expected=b.received;expected.insert(expected.end(),data.begin(),data.end());
 send(b.ipRx,packet(b.seq,b.ack,17,data));b.seq+=130;b.run(1000);
 check(b.received==expected&&get(b.packets.back(),28,4)==b.seq,"S2 FIN payload");
 check(b.closeNotices==c+1,"S2 closed notification with payload");
 unsigned n=b.table.size();t=b.cycles;
 while(b.table.size()==n){check(b.cycles<t+sweeps(66),"S2 TIME_WAIT release missing");b.tick();}
 check(b.closeNotices==c+1,"S2 release notified");

 fin_wait_session(b,60000,40002,"S3 local FIN");c=b.closeNotices;n=b.table.size();
 send(b.ipRx,packet(b.seq,b.ack,4));b.run(500);
 check(b.closeNotices==c+1&&b.table.size()==n-1,"S3 RST after our FIN");

 fin_wait_session(b,70000,40003,"S4 local FIN");c=b.closeNotices;
 send(b.ipRx,packet(b.seq,b.ack,17));b.run(1000);check(b.closeNotices==c+1,"S4 FIN");
 n=b.table.size();
 send(b.ipRx,packet(b.seq+1,b.ack,4));b.run(500);
 check(b.table.size()==n-1,"S4 RST in TIME_WAIT releases");
 check(b.closeNotices>=c+1&&b.closeNotices<=c+2,"S4 notifications after FIN and RST");
 c=b.closeNotices;n=b.table.size();b.run(sweeps(66));
 check(b.closeNotices==c&&b.table.size()==n,"S4 stale close timer activity");

 reopen(b,80000,40004);receive_data(b,65);c=b.closeNotices;
 b.finAck=false;
 b.close.write(b.sid);b.run(500);check(b.fin,"S5 local FIN");
 send(b.ipRx,packet(b.seq,b.ack,17));b.run(1000);
 check(get(b.packets.back(),28,4)==b.seq+1,"S5 peer FIN not ACKed");
 check(b.closeNotices==c+1,"S5 closed notification in CLOSING");
 b.seq+=1;b.ack+=1;
 send(b.ipRx,packet(b.seq,b.ack,16));b.run(500);
 check(b.closeNotices==c+1,"S5 ACK of our FIN notified again");
 n=b.table.size();t=b.cycles;
 while(b.table.size()==n){check(b.cycles<t+sweeps(66),"S5 TIME_WAIT release missing");b.tick();}
 check(b.closeNotices==c+1,"S5 release notified");
 b.finAck=true;
}
int main(){return run_case("close_notify",test);}
