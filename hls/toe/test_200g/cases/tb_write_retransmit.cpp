// Regression: TCP_NODELAY wire delivery can precede completion of buffer writes.
#include "../common/csim.hpp"
static void test() {
 Bench b;connect_passive(b);b.autoAck=false;b.writePauseUntil=b.cycles+2000;
 Bytes data(8960);for(unsigned i=0;i<data.size();i++)data[i]=i*31+9;b.expectedTx=data;
 b.txMeta.write(appTxMeta(b.sid,data.size()));b.run(20);send(b.appTx,data);b.run(300);
 check(b.txPayload==data.size(),"initial bypass TX missing");
 for(int i=0;i<4;i++)send(b.ipRx,packet(b.seq,b.ack,16));
 while(b.cycles<b.writePauseUntil-1)b.tick();
 check(b.readCommands==0,"retransmission read before write commit");
 b.autoAck=true;b.run(3000);check(b.readCommands>0,"deferred retransmission never resumed");
}
int main(){return run_case("write_retransmit",test);}
