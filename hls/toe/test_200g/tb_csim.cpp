#include "common/csim.hpp"
template<int WIDTH> void tasi_pkg_pusher(stream<mmCmd>&, stream<net_axis<WIDTH>>&, stream<mmCmd>&, stream<net_axis<WIDTH>>&);
static void boundary_test() {
 stream<mmCmd> requests,commands;stream<net_axis<DATA_WIDTH>> input,output;
 const unsigned k=DATA_WIDTH/8;
 for(unsigned off=0;off<k;off++) {
  unsigned first=k+off, n=first+k+3;
  requests.write(mmCmd(BUFFER_SIZE-first,n));
  for(unsigned o=0;o<n;o+=k){net_axis<DATA_WIDTH>a(0,0,o+k>=n);for(unsigned j=0;j<k&&o+j<n;j++){a.keep[j]=1;a.data(j*8+7,j*8)=(o+j)*13+7;}input.write(a);}
  for(unsigned c=0;c<30;c++)tasi_pkg_pusher<DATA_WIDTH>(requests,input,commands,output);
  unsigned total=0,seg=0,segment_bytes=0;
  while(!output.empty()) {auto a=output.read();for(unsigned j=0;j<k;j++)if(a.keep[j]){if(a.data(j*8+7,j*8)!=((total*13+7)&255)){std::cerr<<std::dec<<"split off="<<off<<" total="<<total<<" n="<<n<<" keep="<<a.keep.to_string(16)<<std::endl;check(false,"split payload");}total++;segment_bytes++;}if(a.last){check(!commands.empty(),"split command missing");auto cmd=commands.read();check(cmd.bbt==segment_bytes,"split command length");check(cmd.saddr==(seg==0?BUFFER_SIZE-first:0),"split address");segment_bytes=0;seg++;}}
  check(total==n&&seg==2&&commands.empty()&&input.empty(),"split completion");
 }
}
static void window_test() {
#if RX_DDR_BYPASS
 stream<rxSarRecvd> rx;stream<rxSarAppd> app,app_reply;stream<ap_uint<16>> tx;
 stream<rxSarEntry> rx_reply;stream<rxSarReply> tx_reply;
 rx.write(rxSarRecvd(999,0,0,0,false,0));
 rx_sar_table(rx,app,tx,rx_reply,app_reply,tx_reply,0,16);
 for(unsigned used: {0u,3u,8u,11u,12u,16u,17u}) {
  tx.write(999);rx_sar_table(rx,app,tx,rx_reply,app_reply,tx_reply,used,16);
#ifdef V80_RX_WINDOW_MAX
  unsigned expected=0;   // 16 entries are all reserve
#else
  unsigned expected=used>=12?0:(16-used)*(DATA_WIDTH/8);
#endif
  check(tx_reply.read().windowSize==expected,"FIFO entry-to-byte window");
 }
 tx.write(999);rx_sar_table(rx,app,tx,rx_reply,app_reply,tx_reply,0,0);
 check(tx_reply.read().windowSize==0,"zero capacity window");
#ifdef V80_RX_WINDOW_MAX
 // a 4 MiB FIFO: the window is capped below 2^WINDOW_BITS, then follows (free - reserve)/2^SHIFT and is 0
 // inside the reserve; without window scaling (session 999, shift 0) it saturates at 64 KiB - 1, with the
 // TOE's scale (998) it is sent >> 3. 16 sessions' windows together stay out of the reserve.
 rx.write(rxSarRecvd(998,0,0,0,false,WINDOW_SCALE_BITS));
 rx_sar_table(rx,app,tx,rx_reply,app_reply,tx_reply,0,16);
 for(unsigned used: {0u,1000u,20000u,30000u,31000u,31743u,31744u,32000u,32767u,32768u}) {
  unsigned free_entries=used>=32768?0:32768-used;
  unsigned usable=free_entries>V80_RX_WINDOW_RESERVE?free_entries-V80_RX_WINDOW_RESERVE:0;
  unsigned share=(usable*(DATA_WIDTH/8))>>V80_RX_WINDOW_SHIFT;
  check(16*share<=usable*(DATA_WIDTH/8),"windows exceed the free space above the reserve");
  unsigned expected=share<V80_RX_WINDOW_MAX?share:V80_RX_WINDOW_MAX;
  tx.write(999);rx_sar_table(rx,app,tx,rx_reply,app_reply,tx_reply,used,32768);
  check(tx_reply.read().windowSize==(expected<0xFFFF?expected:0xFFFF),"large FIFO window, no scaling");
  tx.write(998);rx_sar_table(rx,app,tx,rx_reply,app_reply,tx_reply,used,32768);
  check(tx_reply.read().windowSize==(expected>>WINDOW_SCALE_BITS),"large FIFO window, scaled");
 }
#endif
#endif
}
static void alignment_test() {
 for(unsigned n=0;n<=DATA_WIDTH/8;n++) {
  auto keep=toeLenToKeep<DATA_WIDTH>(n);
  check(toeKeepToLen<DATA_WIDTH>(keep)==n,"keep length roundtrip");
  for(unsigned i=0;i<DATA_WIDTH/8;i++)check(keep[i]==(i<n),"keep mask");
 }
 for(unsigned offset=0;offset<DATA_WIDTH/8;offset++) {
  net_axis<DATA_WIDTH> a(0,~ap_uint<DATA_WIDTH/8>(0),0),b(0,~ap_uint<DATA_WIDTH/8>(0),1);
  for(unsigned i=0;i<DATA_WIDTH/8;i++){a.data(i*8+7,i*8)=i;b.data(i*8+7,i*8)=i+DATA_WIDTH/8;}
  auto c=toeAlignWords<DATA_WIDTH>(offset,a,b);
  for(unsigned i=0;i<DATA_WIDTH/8;i++)check(c.data(i*8+7,i*8)==((i+offset)&255),"alignment byte");
  check(c.keep==~ap_uint<DATA_WIDTH/8>(0),"alignment keep");
 }
}
static void checksum_test() {
 stream<net_axis<DATA_WIDTH>> in,out;stream<subSums<DATA_WIDTH/16>> sums;stream<ap_uint<16>> result;
 for(unsigned n: {1u,63u,64u,65u,127u,128u,129u,8959u,8960u,8961u}) {
  Bytes b(n);for(unsigned i=0;i<n;i++)b[i]=(i*37+19)&255;
  for(unsigned o=0;o<n;o+=DATA_WIDTH/8){net_axis<DATA_WIDTH>a{};a.data=0;a.keep=0;for(unsigned j=0;j<DATA_WIDTH/8 && o+j<n;j++){a.data(j*8+7,j*8)=b[o+j];a.keep[j]=1;}a.last=o+DATA_WIDTH/8>=n;in.write(a);two_complement_subchecksums<DATA_WIDTH,99>(in,out,sums);out.read();}
  finalize_ipv4_checksum<DATA_WIDTH/16>(sums,result);check(result.read()==checksum(b),"checksum width regression");
 }
}
int main(){try{
 window_test(); alignment_test(); boundary_test(); checksum_test(); Bench b;b.listen.write(5001);b.run(40000);check(!b.listened.empty()&&b.listened.read(),"listen");
 send(b.ipRx,packet(1000,0,2,{},true));b.run(1000);check(b.synack,"SYN-ACK timeout");send(b.ipRx,packet(b.seq,b.ack,16));b.run(300);
 Bytes expected;
 for(unsigned n:{1u,63u,64u,65u,127u,128u,129u,8959u,8960u,8961u}){Bytes data(n);for(unsigned i=0;i<n;i++)data[i]=(expected.size()+i)*17+5;expected.insert(expected.end(),data.begin(),data.end());send(b.ipRx,packet(b.seq,b.ack,24,data));b.seq+=n;b.run(1000);check(b.received==expected,"RX payload mismatch");}
 for(unsigned n:{1u,63u,64u,65u,127u,128u,129u,8959u,8960u,8961u}) {
  Bytes tx(n);for(unsigned i=0;i<n;i++)tx[i]=(b.expectedTx.size()+i)*31+9;
  b.expectedTx.insert(b.expectedTx.end(),tx.begin(),tx.end());b.txMeta.write(appTxMeta(b.sid,n));b.run(20);send(b.appTx,tx);b.run(1000);check(b.txPayload==b.expectedTx.size(),"TX payload timeout");
 }
 // Exercise the actual top across the 256 KiB per-session TX ring boundary.
 for(unsigned packet=0;packet<32;packet++) {
  Bytes tx(8960);for(unsigned i=0;i<tx.size();i++)tx[i]=(b.expectedTx.size()+i)*31+9;
  b.expectedTx.insert(b.expectedTx.end(),tx.begin(),tx.end());b.txMeta.write(appTxMeta(b.sid,tx.size()));b.run(20);send(b.appTx,tx);b.run(1000);check(b.txPayload==b.expectedTx.size(),"TX ring timeout");
 }
 b.close.write(b.sid);b.run(2000);check(b.fin,"FIN timeout");

#ifdef V80_RX_DEBUG
 std::cout<<std::dec<<"RXDEBUG invalid="<<v80_dbg_rx_header<<" port_closed="<<v80_dbg_rx_meta(31,0)<<" lookup_miss="<<v80_dbg_rx_meta(63,32)
  <<" accept="<<v80_dbg_rx_fsm(31,0)<<(RX_DDR_BYPASS?" space=":" ooo=")<<v80_dbg_rx_fsm(63,32)<<" dup="<<v80_dbg_rx_fsm(95,64)<<" gap="<<v80_dbg_rx_fsm(127,96)
  <<" state="<<v80_dbg_rx_fsm(159,128)<<(RX_DDR_BYPASS?" occupancy=":" filled=")<<v80_dbg_rx_fsm(175,160)<<std::endl;
 check(v80_dbg_rx_fsm(31,0)>=10,"RX debug accept counter");
#endif
 std::cout<<std::dec<<"PASS toe width="<<DATA_WIDTH<<" cycles="<<b.cycles<<std::endl;
 std::ofstream("../../csim_pass")<<"PASS\n";return 0;
 }catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}}
