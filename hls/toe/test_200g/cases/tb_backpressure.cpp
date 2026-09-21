// C-sim checks delayed external service. Actual ready/valid stalls are in SV.
#include "../common/csim.hpp"
static void test(){Bench b;b.delay=7;connect_passive(b);for(unsigned n:{1u,63u,64u,65u,127u,128u,129u,8960u}){receive_data(b,n);transmit_data(b,n);}}
int main(){return run_case("backpressure",test);}
