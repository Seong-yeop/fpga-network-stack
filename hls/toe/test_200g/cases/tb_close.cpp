// Linux packetdrill close ordering: local FIN followed by peer FIN with data.
#include "../common/csim.hpp"
static void test() {
 Bench b;connect_passive(b);receive_data(b,65);b.close.write(b.sid);b.run(500);check(b.fin,"local FIN missing");
 Bytes data(129,0x6a),expected=b.received;expected.insert(expected.end(),data.begin(),data.end());
 send(b.ipRx,packet(b.seq,b.ack+1,17,data));b.seq+=130;b.run(1000);
 check(b.received==expected,"FIN payload lost");check(get(b.packets.back(),28,4)==b.seq,"FIN sequence byte not ACKed");
}
int main(){return run_case("close",test);}
