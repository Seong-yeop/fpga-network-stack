#include "../common/csim.hpp"
static std::string fmt(const char *why,const Bench &b){return std::string(why)+" accepted="+std::to_string(b.accepted)+" nospace="+std::to_string(b.noSpace)+" txPayload="+std::to_string(b.txPayload);}
static void test() {
 Bench b;connect_passive(b);b.autoAck=false;b.allowNoSpace=true;
 std::vector<Bytes> segs;
 for(int i=0;i<5;i++){Bytes d=tx_pattern(b,8960);b.expectedTx.insert(b.expectedTx.end(),d.begin(),d.end());segs.push_back(d);}
 unsigned sent=0;
 for(int i=0;i<5;i++)if(try_write(b,segs[sent]))sent++;
 check(sent==2&&b.noSpace==3,fmt("initial window must admit exactly two segments",b).c_str());
 b.run(400);check(b.txPayload==2*8960,fmt("two segments on the wire",b).c_str());
 send(b.ipRx,packet(b.seq,b.ack+8960,16));b.run(200);
 for(int i=0;i<3;i++)if(try_write(b,segs[sent]))sent++;
 check(sent==4,fmt("one acknowledged segment must open one more segment of window",b).c_str());
 b.run(400);check(b.txPayload==4*8960,fmt("four segments on the wire",b).c_str());
 send(b.ipRx,packet(b.seq,b.ack+4*8960,16));b.run(200);
 check(try_write(b,segs[sent]),fmt("window after the cumulative ACK",b).c_str());sent++;
 b.autoAck=true;b.run(600);check(b.txPayload==5*8960,fmt("all segments delivered",b).c_str());
}
int main(){return run_case("initial_window",test);}
