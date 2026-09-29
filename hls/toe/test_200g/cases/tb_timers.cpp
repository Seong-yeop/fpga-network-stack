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
 set.write(txRetransmitTimerSet(9,RT,true));unsigned it=0;bool fastFired=false;
 for(;it<40000;it++){
  retransmit_timer(clear,set,events,release,rx,tx);
  if(!events.empty()){auto e=events.read();check(e.sessionID==9,"fast timer session");check(e.rt_count==0,"fast expiry rt_count");fastFired=true;break;}
 }
 check(fastFired,"fast timer never fired");
 check(it>=2*MAX_SESSIONS&&it<=8*MAX_SESSIONS,"fast timer interval");
 set.write(txRetransmitTimerSet(9));unsigned it2=0;bool fired2=false;
 for(;it2<40000;it2++){
  retransmit_timer(clear,set,events,release,rx,tx);
  if(!events.empty()){auto e=events.read();check(e.rt_count==1,"first RTO expiry rt_count");fired2=true;break;}
 }
 check(fired2,"RTO after the fast expiry never fired");
 check(it2>=8*MAX_SESSIONS&&it2<=20*MAX_SESSIONS,"RTO countdown did not continue across the fast expiry");
 clear.write(rxRetransmitTimerUpdate(9,true));
 for(unsigned i=0;i<4000;i++)retransmit_timer(clear,set,events,release,rx,tx);
 check(events.empty(),"timer did not cancel after the fast path");
 set.write(txRetransmitTimerSet(11,RT,true));
 for(unsigned i=0;i<MAX_SESSIONS;i++)retransmit_timer(clear,set,events,release,rx,tx);
 clear.write(rxRetransmitTimerUpdate(11,false));
 for(unsigned i=0;i<8*MAX_SESSIONS;i++)retransmit_timer(clear,set,events,release,rx,tx);
 check(events.empty(),"an advancing ACK must cancel the fast timer");
 clear.write(rxRetransmitTimerUpdate(11,true));
 for(unsigned i=0;i<4000;i++)retransmit_timer(clear,set,events,release,rx,tx);
 check(events.empty(),"timer did not cancel");
}
int main(){return run_case("timers",test);}
