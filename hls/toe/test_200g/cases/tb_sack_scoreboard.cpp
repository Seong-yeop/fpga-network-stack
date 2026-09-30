#include "../common/csim.hpp"
#include "../../sack_scoreboard/sack_scoreboard.hpp"
static stream<sackRequest> req;static stream<ap_uint<16>> clr;static stream<sackReply> rsp;
static void run(unsigned n){for(unsigned i=0;i<n;i++)sack_scoreboard(req,clr,rsp);}
static ap_uint<256> blocks(const std::vector<std::pair<unsigned,unsigned>> &v){ap_uint<256> b=0;for(unsigned k=0;k<v.size();k++){b(64*k+31,64*k)=v[k].first;b(64*k+63,64*k+32)=v[k].second;}return b;}
static sackReply update(unsigned sid,unsigned ack,const std::vector<std::pair<unsigned,unsigned>> &v){
 req.write(sackRequest(sid,ack,v.size(),blocks(v)));run(16);check(!rsp.empty(),"scoreboard reply missing");sackReply r=rsp.read();check(rsp.empty(),"two replies");return r;
}
static unsigned count(const sackReply &r){return r.valid[0]+r.valid[1]+r.valid[2];}
static bool has(const sackReply &r,unsigned l,unsigned rr){
 return (r.valid[0]&&r.left0==l&&r.right0==rr)||(r.valid[1]&&r.left1==l&&r.right1==rr)||(r.valid[2]&&r.left2==l&&r.right2==rr);
}
static void test() {
 req.write(sackRequest(3));run(8);check(rsp.empty(),"init produced a reply");
 sackReply r=update(3,1000,{{2000,3000}});check(count(r)==1&&has(r,2000,3000)&&r.newData,"first block");
 r=update(3,1000,{{2000,3000}});check(count(r)==1&&has(r,2000,3000)&&!r.newData,"repeated block counted as new");
 r=update(3,1000,{{3000,4000}});check(count(r)==1&&has(r,2000,4000)&&r.newData,"adjacent block not merged");
 r=update(3,1000,{{6000,7000},{9000,9500}});check(count(r)==3&&has(r,2000,4000)&&has(r,6000,7000)&&has(r,9000,9500)&&r.newData,"two new blocks");
 r=update(3,1000,{{12000,12500}});check(count(r)==3&&has(r,12000,12500)&&!has(r,2000,4000)&&r.newData,"full table did not replace the lowest block");
 r=update(3,1000,{{7000,9000}});check(count(r)==2&&has(r,6000,9500)&&has(r,12000,12500)&&r.newData,"bridge block did not coalesce");
 r=update(3,8000,{});check(count(r)==2&&has(r,8000,9500)&&has(r,12000,12500)&&!r.newData,"cumulative ACK did not trim");
 r=update(3,9500,{});check(count(r)==1&&has(r,12000,12500),"block ending at the ACK not dropped");
 r=update(3,9500,{{9000,9400}});check(count(r)==1&&!r.newData,"block below the ACK stored");
 r=update(3,9500,{{13000,12900}});check(count(r)==1&&!r.newData,"inverted block stored");
 r=update(3,9500,{{20000,20000+300000}});check(count(r)==1&&!r.newData,"block beyond the ring stored");
 r=update(3,9500,{{9000,10000}});check(count(r)==2&&has(r,9500,10000)&&r.newData,"block straddling the ACK not clipped");
 clr.write(3);run(8);check(rsp.empty(),"clear produced a reply");
 r=update(3,9500,{});check(count(r)==0,"clear left blocks");
 r=update(3,9500,{{12000,12500}});check(count(r)==1&&r.newData,"block after the clear not new");
 r=update(5,0xffffffe0u,{{0xfffffff0u,0x00000010u}});check(count(r)==1&&has(r,0xfffffff0u,0x10)&&r.newData,"wrap-around block");
 r=update(5,0xfffffff8u,{});check(count(r)==1&&has(r,0xfffffff8u,0x10),"wrap-around trim");
 r=update(4,100,{{200,300},{400,500},{600,700},{800,900}});check(count(r)==3&&r.newData,"four blocks in one ACK");
 check(has(r,400,500)&&has(r,600,700)&&has(r,800,900),"lowest block not evicted");
}
int main(){return run_case("sack_scoreboard",test);}
