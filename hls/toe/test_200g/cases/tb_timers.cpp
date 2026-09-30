#include "../common/csim.hpp"
#include "../../retransmit_timer/retransmit_timer.hpp"
static void test() {
 stream<rxRetransmitTimerUpdate> clear;
 stream<txRetransmitTimerSet> set;
 stream<event> events;
 stream<ap_uint<16>> release;
 stream<appNotification> rx;
 stream<openStatus> tx;
 const unsigned MS=MAX_SESSIONS;
 auto tick=[&](){retransmit_timer(clear,set,events,release,rx,tx);};
 auto drain=[&](unsigned n){for(unsigned i=0;i<n;i++)tick();};
 auto wait_event=[&](unsigned sid,unsigned count,unsigned timeout){
  unsigned elapsed=0;
  for(;elapsed<timeout&&events.empty();elapsed++)tick();
  check(!events.empty(),"RTO event missing");
  auto e=events.read();
  check(e.sessionID==sid&&e.rt_count==count,"retry event mismatch");
  return elapsed;
 };
 auto expect_none=[&](unsigned n){drain(n);check(events.empty()&&release.empty(),"unexpected timer output");};
 auto in_window=[&](unsigned elapsed,unsigned lo,unsigned hi,const char *why){check(elapsed>=lo*MS&&elapsed<hi*MS,why);};
 set.write(txRetransmitTimerSet(7));
 in_window(wait_event(7,1,18*MS),15,18,"first RTO outside 16 sweeps");
 set.write(txRetransmitTimerSet(7));
 drain(5*MS);
 clear.write(rxRetransmitTimerUpdate(7,true,false));
 expect_none(70*MS);
 check(rx.empty(),"stopped timer notified the application");
 set.write(txRetransmitTimerSet(9));
 drain(8*MS);
 set.write(txRetransmitTimerSet(9));
 unsigned e=wait_event(9,1,10*MS);
 check(e>=7*MS&&e<10*MS,"set on an active timer extended the deadline");
 set.write(txRetransmitTimerSet(9));
 in_window(wait_event(9,2,34*MS),31,34,"set after an expiry did not use the backed-off RTO");
 clear.write(rxRetransmitTimerUpdate(9,true,false));
 expect_none(3*MS);
 set.write(txRetransmitTimerSet(11));
 in_window(wait_event(11,1,18*MS),15,18,"retry 1 outside 16 sweeps");
 set.write(txRetransmitTimerSet(11));
 in_window(wait_event(11,2,34*MS),31,34,"retry 2 outside 32 sweeps");
 set.write(txRetransmitTimerSet(11));
 drain(10*MS);
 clear.write(rxRetransmitTimerUpdate(11,false,false));
 in_window(wait_event(11,1,62*MS),59,62,"restart without collapse lost the backoff");
 set.write(txRetransmitTimerSet(11));
 drain(10*MS);
 clear.write(rxRetransmitTimerUpdate(11,false,true));
 in_window(wait_event(11,1,18*MS),15,18,"restart with collapse kept the backoff");
 clear.write(rxRetransmitTimerUpdate(11,true,false));
 expect_none(70*MS);
 set.write(txRetransmitTimerSet(13));
 in_window(wait_event(13,1,18*MS),15,18,"session 13 first RTO");
 clear.write(rxRetransmitTimerUpdate(13,false,false));
 in_window(wait_event(13,1,34*MS),31,34,"advancing ACK did not arm the expired timer with the backed-off RTO");
 clear.write(rxRetransmitTimerUpdate(13,true,false));
 expect_none(70*MS);
 const unsigned lo[]={15,31,59,59,59,59},hi[]={18,34,62,62,62,62};
 for(unsigned n=0;n<6;n++){
  set.write(txRetransmitTimerSet(12));
  in_window(wait_event(12,n+1,hi[n]*MS),lo[n],hi[n],"retry schedule mismatch");
  check(release.empty(),"connection released before six retransmissions");
 }
 set.write(txRetransmitTimerSet(12));
 drain(70*MS);
 check(events.empty(),"more than six retransmissions");
 check(!release.empty()&&release.read()==12,"retry limit did not release the session");
 check(!rx.empty()&&rx.read().sessionID==12,"data timeout notification missing");
 check(tx.empty(),"data timeout produced an active-open notification");
 for(unsigned n=0;n<6;n++){
  set.write(txRetransmitTimerSet(14,SYN));
  in_window(wait_event(14,n+1,hi[n]*MS),lo[n],hi[n],"SYN retry schedule mismatch");
 }
 set.write(txRetransmitTimerSet(14,SYN));
 drain(70*MS);
 check(!release.empty()&&release.read()==14,"SYN retry limit did not release the session");
 check(!tx.empty()&&!tx.read().success,"SYN timeout notification missing");
 check(rx.empty(),"SYN timeout produced a data notification");
 set.write(txRetransmitTimerSet(20));
 tick();
 clear.write(rxRetransmitTimerUpdate(20,true,false));
 expect_none(70*MS);
 clear.write(rxRetransmitTimerUpdate(21,true,false));
 tick();
 set.write(txRetransmitTimerSet(21));
 in_window(wait_event(21,1,18*MS),15,18,"set after an adjacent stop did not arm");
 clear.write(rxRetransmitTimerUpdate(21,true,false));
 expect_none(20*MS);
 std::vector<unsigned> armed(MS,0),seen(MS,0);
 unsigned now=0;
 for(unsigned id=0;id<MS;id++){set.write(txRetransmitTimerSet(id));armed[id]=now;tick();now++;}
 for(unsigned n=0;n<20*MS;n++){
  tick();now++;
  while(!events.empty()){auto ev=events.read();unsigned id=ev.sessionID;check(id<MS&&ev.rt_count==1&&seen[id]==0,"unexpected event in the all-session arm");seen[id]=1;check(now-armed[id]>=15*MS&&now-armed[id]<19*MS,"all-session arm fired outside 16 sweeps");}
 }
 for(unsigned id=0;id<MS;id++)check(seen[id]==1,"a session armed in consecutive cycles never fired");
 check(release.empty(),"all-session arm released a session");
 for(unsigned id=0;id<MS;id++){clear.write(rxRetransmitTimerUpdate(id,true,false));tick();}
 expect_none(70*MS);
 for(unsigned id=0;id<MS;id++){set.write(txRetransmitTimerSet(id));tick();}
 drain(5*MS);
 std::fill(seen.begin(),seen.end(),0);now=0;
 for(unsigned id=0;id<MS;id++){clear.write(rxRetransmitTimerUpdate(id,false,true));armed[id]=now;tick();now++;}
 for(unsigned n=0;n<20*MS;n++){
  tick();now++;
  while(!events.empty()){auto ev=events.read();unsigned id=ev.sessionID;check(id<MS&&ev.rt_count==1&&seen[id]==0,"unexpected event in the all-session restart");seen[id]=1;check(now-armed[id]>=15*MS&&now-armed[id]<19*MS,"all-session restart fired outside 16 sweeps");}
 }
 for(unsigned id=0;id<MS;id++)check(seen[id]==1,"a session restarted in consecutive cycles never fired");
 for(unsigned id=0;id<MS;id++){clear.write(rxRetransmitTimerUpdate(id,true,false));tick();}
 expect_none(70*MS);
}
int main(){return run_case("timers",test);}
