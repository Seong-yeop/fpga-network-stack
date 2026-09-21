// Existing TX SAR request ports provide wrap-boundary state without RTL forcing.
#include "../common/csim.hpp"
#include "../../tx_sar_table/tx_sar_table.hpp"
static void test() {
 stream<rxTxSarQuery> rx;stream<txTxSarQuery> tx;stream<txAppTxSarPush> app;
 stream<rxTxSarReply> rr;stream<txTxSarReply> tr;stream<txSarAckPush> ack;
 auto tick=[&](){tx_sar_table(rx,tx,app,rr,tr,ack);};
 tx.write(txTxSarQuery(7,0xfffffff0u,1,1));tick();
 tx.write(txTxSarQuery(7,0x80,1));tick();
 rx.write(rxTxSarQuery(7,0x40,65535,14600,0,false,3));tick();
 tx.write(txTxSarQuery(7));tick();check(!tr.empty(),"TX SAR reply missing");auto r=tr.read();
 check(r.ackd==0x40 && r.not_ackd==0x80 && r.usedLength==64,"TX SAR wrap accounting");
 check(r.writesPending,"uncommitted wrap marked readable");
 app.write(txAppTxSarPush(7,0x80));tick();tx.write(txTxSarQuery(7));tick();
 check(!tr.empty()&&!tr.read().writesPending,"commit did not clear wrap hazard");
}
int main(){return run_case("tx_sequence_wrap",test);}
