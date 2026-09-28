// ns-3 timer scenarios; C-sim's existing short constants, not wall-clock RTO.
#include "../common/csim.hpp"
#include "../../retransmit_timer/retransmit_timer.hpp"
static void test() {
 stream<rxRetransmitTimerUpdate> clear;stream<txRetransmitTimerSet> set;
 stream<event> events;stream<ap_uint<16>> release;stream<appNotification> rx;stream<openStatus> tx;
 set.write(txRetransmitTimerSet(7));bool fired=false;
 for(unsigned i=0;i<40000;i++) {
  retransmit_timer(clear,set,events,release,rx,tx);
  if(!events.empty()){auto e=events.read();check(e.sessionID==7,"wrong timer session");fired=true;clear.write(rxRetransmitTimerUpdate(7,true));break;}
 }
 check(fired,"RTO event missing");
 for(unsigned i=0;i<4000;i++)retransmit_timer(clear,set,events,release,rx,tx);
 check(events.empty(),"timer did not cancel");
}
int main(){return run_case("timers",test);}
