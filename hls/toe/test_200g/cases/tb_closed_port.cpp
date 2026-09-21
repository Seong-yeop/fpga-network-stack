#include "../common/csim.hpp"
static void test() {
 Bench b;b.run(40000);send(b.ipRx,packet(1000,0,2,{},true));b.run(1000);
 check(!b.packets.empty()&&(b.packets.back()[33]&4),"closed port did not reset SYN");
 connect_passive(b);receive_data(b,65);
}
int main(){return run_case("closed_port",test);}
