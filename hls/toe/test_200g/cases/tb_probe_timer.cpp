// Existing short C-sim constants; expiry and RX-triggered fast resume are events.
#include "../common/csim.hpp"
#include "../../probe_timer/probe_timer.hpp"
static void test() {
 stream<ap_uint<16>> resume,set;stream<event> events;
 auto tick=[&](){probe_timer(resume,set,events);};
 set.write(9);for(int i=0;i<3;i++)tick();resume.write(9);tick();
 check(!events.empty(),"probe fast-resume missing");auto e=events.read();check(e.sessionID==9 && e.type==RT,"wrong probe resume event");
 for(int i=0;i<4000;i++)tick();check(events.empty(),"probe repeated after resume");
 set.write(7);bool expired=false;
 for(int i=0;i<10000;i++){tick();if(!events.empty()){e=events.read();check(e.sessionID==7 && e.type==RT,"wrong probe expiry");expired=true;break;}}
 check(expired,"probe expiry missing");for(int i=0;i<4000;i++)tick();check(events.empty(),"probe expiry not one-shot");
}
int main(){return run_case("probe_timer",test);}
