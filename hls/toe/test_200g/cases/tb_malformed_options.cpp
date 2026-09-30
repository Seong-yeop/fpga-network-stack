#include "../common/csim.hpp"
#include "../../rx_engine/rx_engine.hpp"
void parse_optional_header_fields(stream<ap_uint<4>>&,stream<ap_uint<320>>&,stream<rxTcpOptions>&);
static rxTcpOptions parse(ap_uint<320> bytes,unsigned words) {
 stream<ap_uint<4>> length;stream<ap_uint<320>> fields;stream<rxTcpOptions> result;
 length.write(words);fields.write(bytes);
 for(int i=0;i<64;i++)parse_optional_header_fields(length,fields,result);
 check(!result.empty(),"option parser stalled");
 rxTcpOptions o=result.read();check(result.empty(),"option parser produced two results");return o;
}
static void test() {
 for(unsigned option_len:{0u,1u,5u}) {
  ap_uint<320> bytes=0;bytes(7,0)=30;bytes(15,8)=option_len;
  rxTcpOptions o=parse(bytes,1);
  check(o.winScale==0&&o.sackOk==0&&o.sackCount==0,"invalid option enabled an option");
 }
 ap_uint<320> good=0;good(7,0)=1;good(15,8)=3;good(23,16)=3;good(31,24)=7;
 check(parse(good,1).winScale==7,"parser did not recover");
 ap_uint<320> syn=0;syn(7,0)=2;syn(15,8)=4;syn(23,16)=0x23;syn(31,24)=0x00;syn(39,32)=4;syn(47,40)=2;syn(55,48)=1;syn(63,56)=1;syn(71,64)=1;syn(79,72)=3;syn(87,80)=3;syn(95,88)=2;
 rxTcpOptions s=parse(syn,3);
 check(s.sackOk==1&&s.winScale==2&&s.sackCount==0,"MSS, SACK-permitted, NOPs and WS were not all parsed");
 ap_uint<320> badSack=0;badSack(7,0)=4;badSack(15,8)=3;badSack(23,16)=3;badSack(31,24)=3;badSack(39,32)=5;
 rxTcpOptions b=parse(badSack,1);
 check(b.sackOk==0&&b.winScale==0,"SACK-permitted with a wrong length was accepted");
 ap_uint<320> sack=0;sack(7,0)=1;sack(15,8)=1;sack(23,16)=5;sack(31,24)=18;
 sack(63,32)=0x11223344;sack(95,64)=0x55667788;sack(127,96)=0x99aabbcc;sack(159,128)=0xddeeff00;
 rxTcpOptions k=parse(sack,5);
 check(k.sackCount==2,"two SACK blocks expected");
 check(k.sack(31,0)==0x44332211&&k.sack(63,32)==0x88776655,"first SACK block bytes not reversed");
 check(k.sack(95,64)==0xccbbaa99&&k.sack(127,96)==0x00ffeedd,"second SACK block bytes not reversed");
 ap_uint<320> odd=0;odd(7,0)=5;odd(15,8)=12;
 rxTcpOptions d=parse(odd,3);
 check(d.sackCount==0,"SACK option with a length that is not 2+8n was accepted");
 ap_uint<320> tail=0;tail(7,0)=1;tail(15,8)=1;tail(23,16)=1;tail(31,24)=1;
 rxTcpOptions t=parse(tail,1);
 check(t.winScale==0&&t.sackCount==0,"NOP-only options changed state");
}
int main(){return run_case("malformed_options",test);}
