#include "../common/csim.hpp"
#include <set>
#include <string>
#include <cstdio>
struct Conn {
 unsigned port=0,myPort=0,peerSeq=0,toeNext=0,sid=0xffff,finSeq=0,fins=0,rsts=0,synSeen=0,synAck=0,peerFin=0,peerFinAcked=0;
 long released=-1;unsigned t0=0;
};
using Group=std::vector<Conn*>;
static const unsigned MS=MAX_SESSIONS;
static unsigned T60() {return TIME_60s.to_uint()*MS;}
static bool anyFail=false;
static std::vector<std::string> summary;
static std::set<unsigned> releasedSids;
static unsigned sw16(unsigned p) {return ((p&255)<<8)|((p>>8)&255);}
static unsigned sid_of(const Bench &b,const Conn &c) {
 for(auto &e:b.table)if(e.first.theirPort==sw16(c.port)&&e.first.myPort==sw16(c.myPort))return e.second;
 return 0xffff;
}
static void scan(Bench &b,Group &cs) {
 while(!b.packets.empty()) {
  Bytes p=b.packets.front();b.packets.pop_front();
  unsigned src=get(p,20),dst=get(p,22),seq=get(p,24,4),ackn=get(p,28,4),fl=p[33],hl=(p[32]>>4)*4,len=p.size()-20-hl;
  for(Conn *c:cs) {
   if(c->port!=dst||(c->myPort&&c->myPort!=src&&!(fl&2)))continue;
   if(fl&2){c->myPort=src;c->toeNext=seq+1;if(fl&16)c->synAck++;else c->synSeen++;}
   if(len&&int(seq+len-c->toeNext)>0)c->toeNext=seq+len;
   if(fl&1){c->fins++;c->finSeq=seq+len;c->toeNext=seq+len+1;}
   if(fl&4)c->rsts++;
   if((fl&16)&&c->peerFin&&ackn==c->peerFin+1)c->peerFinAcked++;
  }
 }
}
static void step(Bench &b,Group &cs,unsigned n=1) {
 for(unsigned i=0;i<n;i++) {
  b.tick();scan(b,cs);
  for(Conn *c:cs)if(c->released<0&&c->sid!=0xffff&&sid_of(b,*c)==0xffff)c->released=b.cycles;
 }
}
template<class F> static bool wait_for(Bench &b,Group &cs,unsigned limit,F f) {
 unsigned end=b.cycles+limit;
 while(!f()){if(b.cycles>=end)return false;step(b,cs);}
 return true;
}
static void tx(Bench &b,const Conn &c,unsigned seq,unsigned ack,unsigned flags) {
 unsigned keep=peerPort;peerPort=c.port;send(b.ipRx,packet(seq,ack,flags,{},false,c.myPort));peerPort=keep;
}
static void popen(Bench &b,Group &cs,Conn &c,unsigned port,unsigned isn) {
 c=Conn();c.port=port;c.peerSeq=isn+1;cs.push_back(&c);
 unsigned keep=peerPort;peerPort=port;send(b.ipRx,packet(isn,0,2,{},true));peerPort=keep;
 check(wait_for(b,cs,3000,[&]{return c.synAck>0||c.rsts>0;}),"passive open: no SYN-ACK");
 check(c.synAck>0,"passive open: RST instead of SYN-ACK");
 tx(b,c,c.peerSeq,c.toeNext,16);step(b,cs,200);
 c.sid=sid_of(b,c);check(c.sid!=0xffff,"passive open: no lookup entry");
}
static void aopen(Bench &b,Group &cs,Conn &c,unsigned port,unsigned isn) {
 c=Conn();c.port=port;cs.push_back(&c);unsigned r=b.openReplies;
 ipTuple t;t.ip_address=0x0a000002;t.ip_port=port;b.open.write(t);
 check(wait_for(b,cs,3000,[&]{return c.synSeen>0;}),"active open: no SYN");
 c.peerSeq=isn+1;
 unsigned keep=peerPort;peerPort=port;send(b.ipRx,packet(isn,c.toeNext,18,{},true,c.myPort));peerPort=keep;
 check(wait_for(b,cs,3000,[&]{return b.openReplies>r;}),"active open: no open reply");
 unsigned osid=b.sid;step(b,cs,200);
 c.sid=sid_of(b,c);check(c.sid!=0xffff,"active open: no lookup entry");
 check(c.sid==osid,"active open: open reply session ID differs from the lookup entry");
}
static void close_all(Bench &b,Group &cs,const Group &g) {
 for(Conn *c:g)b.close.write(c->sid);
 check(wait_for(b,cs,6000,[&]{for(Conn *c:g)if(!c->fins)return false;return true;}),"no FIN after the close request");
}
static void peer_fin(Bench &b,const Conn &c0,unsigned ack) {
 Conn &c=const_cast<Conn&>(c0);c.peerFin=c.peerSeq;tx(b,c,c.peerSeq,ack,17);c.peerSeq++;
}
static void record(const std::string &name,bool ok,const std::string &detail) {
 std::string line=std::string("TW ")+(ok?"PASS ":"FAIL ")+name+" "+detail;
 std::cout<<line<<std::endl;summary.push_back(line);if(!ok)anyFail=true;
}
static void dump(const Bench &b,const char *why) {
 std::cout<<"TW-DUMP "<<why<<" cycle="<<b.cycles<<" table="<<b.table.size()<<" closeNotices="<<b.closeNotices<<std::endl;
 for(auto &e:b.table)std::cout<<"TW-DUMP  sid="<<e.second<<" theirPort="<<sw16(e.first.theirPort.to_uint())<<" myPort="<<sw16(e.first.myPort.to_uint())<<std::endl;
}
static void judge_timeout(Bench &b,Group &cs,const Group &g,const std::string &name,bool needAck) {
 unsigned lo=T60()*9/10,hi=T60()*11/10+2000;
 unsigned last=0;for(Conn *c:g)last=std::max(last,c->t0);
 wait_for(b,cs,last+4*T60()-b.cycles,[&]{for(Conn *c:g)if(c->released<0)return false;return true;});
 bool ok=true;std::string d;
 for(Conn *c:g) {
  long dt=c->released<0?-1:c->released-long(c->t0);
  d+="sid"+std::to_string(c->sid)+":"+(c->released<0?std::string("never"):std::to_string(dt));
  if(c->released>=0)releasedSids.insert(c->sid);
  if(needAck&&!c->peerFinAcked){ok=false;d+="(peer FIN not ACKed)";}
  if(c->released<0||dt<long(lo)||dt>long(hi)){ok=false;d+="(outside "+std::to_string(lo)+".."+std::to_string(hi)+")";}
  d+=" ";
 }
 record(name,ok,d);
 if(!ok)dump(b,name.c_str());
}
static void s_passive_close(Bench &b) {
 Group cs;Conn c;popen(b,cs,c,41000,3000);
 peer_fin(b,c,c.toeNext);
 bool fin=wait_for(b,cs,3000,[&]{return c.fins>0;});
 tx(b,c,c.peerSeq,c.finSeq+1,16);c.t0=b.cycles;
 wait_for(b,cs,T60()*2,[&]{return c.released>=0;});
 long dt=c.released<0?-1:c.released-long(c.t0);
 if(c.released>=0)releasedSids.insert(c.sid);
 bool ok=fin&&c.peerFinAcked&&c.released>=0&&dt<2000;
 record("passive_close",ok,"sid"+std::to_string(c.sid)+" fin="+std::to_string(c.fins)+" peerFinAcked="+std::to_string(c.peerFinAcked)+" release_after_last_ack="+(c.released<0?std::string("never"):std::to_string(dt)));
 if(!ok)dump(b,"passive_close");
}
static void s_single(Bench &b,const char *name,bool active,unsigned port,int order,unsigned data) {
 Group cs;Conn c;
 try {
  if(active)aopen(b,cs,c,port,4000);else popen(b,cs,c,port,4000);
  if(data) {
   peerPort=c.port;b.seq=c.peerSeq;b.ack=c.toeNext;b.myPort=c.myPort;b.sid=c.sid;b.expectedTx.clear();b.txPayload=0;b.autoAck=(order!=1);
   write_data(b,data,0);wait_tx(b,data,"TX data missing");scan(b,cs);b.autoAck=true;
  }
  close_all(b,cs,{&c});
  if(order==0) {
   tx(b,c,c.peerSeq,c.finSeq+1,16);step(b,cs,10*MS);
   peer_fin(b,c,c.finSeq+1);c.t0=b.cycles;
  } else if(order==1) {
   peer_fin(b,c,c.finSeq+1);c.t0=b.cycles;
  } else {
   peer_fin(b,c,c.finSeq);step(b,cs,2*MS);
   tx(b,c,c.peerSeq,c.finSeq+1,16);c.t0=b.cycles;
  }
  judge_timeout(b,cs,{&c},name,true);
 } catch(const std::exception &e) {record(name,false,e.what());dump(b,name);}
}
static void s_eight(Bench &b,const char *name,bool active,unsigned port,int order) {
 Group cs;std::vector<Conn> cv(8);Group g;
 try {
  for(unsigned i=0;i<8;i++){if(active)aopen(b,cs,cv[i],port+i,5000+i*100);else popen(b,cs,cv[i],port+i,5000+i*100);g.push_back(&cv[i]);}
  close_all(b,cs,g);
  if(order==0) {
   for(Conn *c:g)tx(b,*c,c->peerSeq,c->finSeq+1,16);
   step(b,cs,10*MS);
   for(Conn *c:g){peer_fin(b,*c,c->finSeq+1);c->t0=b.cycles;}
  } else {
   for(Conn *c:g){peer_fin(b,*c,c->finSeq+1);c->t0=b.cycles;}
  }
  judge_timeout(b,cs,g,name,true);
 } catch(const std::exception &e) {record(name,false,e.what());dump(b,name);}
}
static void s_reuse(Bench &b) {
 const std::set<unsigned> &freed=releasedSids;
 unsigned before=b.table.size();
 Group cs;std::deque<Conn> cv;std::set<unsigned> sids;unsigned refused=0;
 for(unsigned i=0;i<MS;i++) {
  cv.emplace_back();Conn &c=cv.back();c.port=50000+i;c.peerSeq=9001;cs.clear();cs.push_back(&c);
  unsigned keep=peerPort;peerPort=c.port;send(b.ipRx,packet(9000,0,2,{},true));peerPort=keep;
  wait_for(b,cs,3000,[&]{return c.synAck>0||c.rsts>0;});
  if(!c.synAck){refused++;continue;}
  tx(b,c,c.peerSeq,c.toeNext,16);step(b,cs,60);
  unsigned s=sid_of(b,c);if(s!=0xffff)sids.insert(s);
 }
 unsigned missing=0;for(unsigned s:freed)if(!sids.count(s))missing++;
 bool ok=before==0&&refused==0&&sids.size()==MS&&missing==0;
 record("reuse_all_ids",ok,"entries_before="+std::to_string(before)+" accepted="+std::to_string(MS-refused)+" refused="+std::to_string(refused)+" distinct_sids="+std::to_string(sids.size())+" released_ids="+std::to_string(freed.size())+" released_ids_not_reused="+std::to_string(missing));
}
static void test() {
 Bench b;b.finAck=false;
 std::cout<<"TW MAX_SESSIONS="<<MS<<" TIME_60s="<<TIME_60s.to_uint()<<" T60_cycles="<<T60()<<std::endl;
 b.listen.write(5001);b.run(40000);check(!b.listened.empty()&&b.listened.read(),"listen");
 s_passive_close(b);
 s_single(b,"po_fw2",false,41100,0,0);
 s_single(b,"po_finack",false,41200,1,0);
 s_single(b,"po_closing",false,41300,2,0);
 s_single(b,"ao_fw2_data",true,5201,0,4000);
 s_single(b,"ao_finack_data_unacked",true,5202,1,4000);
 s_single(b,"ao_closing",true,5203,2,0);
 s_eight(b,"ao8_finack",true,5300,1);
 s_eight(b,"ao8_fw2",true,5400,0);
 s_eight(b,"po8_finack",false,42000,1);
 s_reuse(b);
 std::cout<<"TW SUMMARY"<<std::endl;for(auto &l:summary)std::cout<<l<<std::endl;
 check(!anyFail,"one or more orderings failed");
}
int main(){return run_case("timewait_release",test);}
