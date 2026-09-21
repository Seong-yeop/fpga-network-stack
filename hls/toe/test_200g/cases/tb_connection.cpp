// packetdrill: duplicate SYN and invalid RST must preserve a live connection.
#include "../common/csim.hpp"
static void test() {
 Bench b;b.listen.write(5001);b.run(40000);
 send(b.ipRx,packet(1000,0,2,{},true));b.run(300);check(b.synack,"SYN-ACK missing");unsigned original=b.ack;
 send(b.ipRx,packet(1000,0,2,{},true));b.run(300);check(b.ack==original,"duplicate SYN changed ISN");
 send(b.ipRx,packet(b.seq,b.ack,16));b.run(300);receive_data(b,65);
 send(b.ipRx,packet(b.seq+10,b.ack,4));b.run(300);receive_data(b,129);
 send(b.ipRx,packet(b.seq,b.ack,4));b.run(500);
 unsigned before=b.received.size();send(b.ipRx,packet(b.seq,b.ack,24,Bytes(64,1)));b.run(500);
 check(b.received.size()==before,"data delivered after valid RST");
 check(!b.packets.empty(),"no response after closed connection");
}
int main(){return run_case("connection",test);}
