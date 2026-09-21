// Peer ISN crosses the wire's 32-bit boundary without changing DUT internals.
#include "../common/csim.hpp"
static void test() {
 Bench b;connect_passive(b,0xfffffff0u);
 for(unsigned n:{1u,15u,63u,64u,65u,127u,128u,129u,8960u})receive_data(b,n);
 transmit_data(b,8960);
}
int main(){return run_case("sequence_wrap",test);}
