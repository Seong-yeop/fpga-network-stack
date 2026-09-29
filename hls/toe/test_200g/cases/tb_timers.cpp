#include "../common/csim.hpp"
#include "../../retransmit_timer/retransmit_timer.hpp"
static void test() {
 stream<rxRetransmitTimerUpdate> clear;
 stream<txRetransmitTimerSet> set;
 stream<event> events;
 stream<ap_uint<16>> release;
 stream<appNotification> rx;
 stream<openStatus> tx;
 auto tick=[&](){retransmit_timer(clear,set,events,release,rx,tx);};
 auto drain=[&](unsigned n){for(unsigned i=0;i<n;i++)tick();};
 auto wait_event=[&](unsigned sid,unsigned count,unsigned timeout){
  unsigned elapsed=0;
  for(;elapsed<timeout&&events.empty();elapsed++)tick();
  check(!events.empty(),"original RTO event missing");
  auto e=events.read();
  check(e.sessionID==sid&&e.rt_count==count,"original retry event mismatch");
  return elapsed;
 };
 set.write(txRetransmitTimerSet(7));
 wait_event(7,1,(TIME_1s.to_uint()+3)*MAX_SESSIONS);
 clear.write(rxRetransmitTimerUpdate(7,true));
 drain((TIME_30s.to_uint()+3)*MAX_SESSIONS);
 check(events.empty()&&release.empty(),"stopped timer remained active");
 set.write(txRetransmitTimerSet(9));
 drain(8);
 for(unsigned i=0;i<8*MAX_SESSIONS;i++){
  if(i%(MAX_SESSIONS/2)==0)clear.write(rxRetransmitTimerUpdate(9,false));
  tick();
  check(events.empty(),"original ACK update did not restart the timer");
 }
 wait_event(9,1,(TIME_1s.to_uint()+3)*MAX_SESSIONS);
 clear.write(rxRetransmitTimerUpdate(9,true));
 drain(3*MAX_SESSIONS);
 const unsigned intervals[]={TIME_1s.to_uint(),TIME_5s.to_uint(),TIME_10s.to_uint(),TIME_15s.to_uint()};
 for(unsigned n=0;n<4;n++){
  set.write(txRetransmitTimerSet(12));
  unsigned elapsed=wait_event(12,n+1,(intervals[n]+3)*MAX_SESSIONS);
  check(elapsed>=intervals[n]*MAX_SESSIONS,"original retry expired early");
  check(release.empty(),"connection released before four retransmissions");
 }
 set.write(txRetransmitTimerSet(12));
 drain((TIME_30s.to_uint()+3)*MAX_SESSIONS);
 check(events.empty(),"more than four retransmissions");
 check(!release.empty()&&release.read()==12,"original retry limit did not release the session");
 check(!rx.empty()&&rx.read().sessionID==12,"data timeout notification missing");
 check(tx.empty(),"data timeout produced an active-open notification");
 for(unsigned n=0;n<4;n++){
  set.write(txRetransmitTimerSet(14,SYN));
  wait_event(14,n+1,(intervals[n]+3)*MAX_SESSIONS);
 }
 set.write(txRetransmitTimerSet(14,SYN));
 drain((TIME_30s.to_uint()+3)*MAX_SESSIONS);
 check(!release.empty()&&release.read()==14,"SYN retry limit did not release the session");
 check(!tx.empty()&&!tx.read().success,"SYN timeout notification missing");
}
int main(){return run_case("timers",test);}
