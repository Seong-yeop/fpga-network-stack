// packetdrill tcp_basic_client: app open -> SYN -> SYN-ACK -> established.
#include "../common/csim.hpp"
static void test() {
 Bench b;b.run(40000);ipTuple target;target.ip_address=0x0a000002;target.ip_port=40000;b.open.write(target);b.run(300);
 check(!b.packets.empty(),"active SYN missing");Bytes syn=b.packets.front();check(syn[33]==2,"active open did not send SYN");
 Bytes reply=packet(1000,get(syn,24,4)+1,18,{},true);put(reply,22,get(syn,20));reply=resign(reply);send(b.ipRx,reply);b.run(1000);
 check(b.openReplies==1,"open result missing");receive_data(b,129);transmit_data(b,128);
}
int main(){return run_case("active_open",test);}
