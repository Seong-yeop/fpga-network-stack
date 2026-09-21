// Unknown options with zero/oversized lengths must not trap the parser.
#include "../common/csim.hpp"
void parse_optional_header_fields(stream<ap_uint<4>>&,stream<ap_uint<320>>&,stream<ap_uint<4>>&);
static void test() {
 stream<ap_uint<4>> length,result;stream<ap_uint<320>> fields;
 for(unsigned option_len:{0u,1u,5u}) {
  ap_uint<320> bytes=0;bytes(7,0)=30;bytes(15,8)=option_len;
  length.write(1);fields.write(bytes);
  for(int i=0;i<16;i++)parse_optional_header_fields(length,fields,result);
  check(!result.empty(),"malformed option parser stalled");check(result.read()==0,"invalid option enabled WS");
 }
 ap_uint<320> good=0;good(7,0)=1;good(15,8)=3;good(23,16)=3;good(31,24)=7;
 length.write(1);fields.write(good);
 for(int i=0;i<16;i++)parse_optional_header_fields(length,fields,result);
 check(!result.empty()&&result.read()==7,"parser did not recover");
}
int main(){return run_case("malformed_options",test);}
