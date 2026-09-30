#include "../common/csim.hpp"
static void handshake(Bench &b,bool sack,bool ws,unsigned isn) {
 b.expectWS=ws;b.sackOk=sack;b.seq=isn+1;b.listen.write(5001);b.run(40000);check(!b.listened.empty()&&b.listened.read(),"listen timeout");
 send(b.ipRx,packet_syn(isn,sack,ws?3:-1));b.run(1000);check(b.synack,"SYN-ACK timeout");
 Bytes synack=b.packets.back();
 unsigned optlen=4+(ws?4:0)+(sack?4:0);
 check(synack.size()==40+optlen,"SYN-ACK size");
 check((synack[32]>>4)==5+optlen/4,"SYN-ACK data offset");
 check(synack[40]==2&&synack[41]==4&&get(synack,42)==8960,"SYN-ACK MSS");
 unsigned p=44;
 if(ws){check(synack[44]==3&&synack[45]==3&&synack[46]==3,"SYN-ACK WS");p=47;}
 if(sack){check(synack[p]==4&&synack[p+1]==2,"SYN-ACK SACK-permitted");for(unsigned k=p+2;k<synack.size();k++)check(synack[k]==1,"SYN-ACK padding");}
 else if(ws)check(synack[47]==0,"SYN-ACK end of options");
 send(b.ipRx,packet(b.seq,b.ack,16));b.run(300);b.packets.clear();
}
static void test() {
 {Bench b;handshake(b,true,true,1000);receive_data(b,129);transmit_data(b,8960);}
 {Bench b;handshake(b,true,false,2000);receive_data(b,129);transmit_data(b,8960);}
 {Bench b;handshake(b,false,true,3000);receive_data(b,129);transmit_data(b,8960);}
 {
  Bench b;connect_passive(b);
  ipTuple target;target.ip_address=0x0a000002;target.ip_port=40000;b.open.write(target);b.run(300);
  check(!b.packets.empty(),"active SYN missing");Bytes syn=b.packets.back();
  check(syn[33]==2&&syn.size()==52&&(syn[32]>>4)==8,"active SYN options");
  check(syn[40]==2&&syn[44]==3&&syn[47]==4&&syn[48]==2&&syn[49]==1&&syn[50]==1&&syn[51]==1,"active SYN option layout");
 }
}
int main(){return run_case("sack_negotiate",test);}
