int issued=0;
task automatic case_init;
 auto_ack=0;retransmit_armed=1;max_cycles=600000;
endtask
task automatic case_step;
 if(connected[0] && cycle>45000)case(case_phase)
  0:if(!tx_pending[0] && retry_len[0]==0 && issued<5)begin request_tx(0,8960);issued++;end
    else if(issued>=3 && cycle>60000)begin case_start=cycle;case_phase=1;end
  1:if(cycle-case_start>30000)begin
   if(tx_count[0]!=2*8960)$fatal(1,"initial window sent %0d bytes, expected two segments",tx_count[0]);
   if(tx_retries==0)$fatal(1,"the third segment was never rejected");
   send_packet(0,peer_seq[0],server_seq[0]+8960,16);case_start=cycle;case_phase=2;
  end
  2:if(!tx_pending[0] && retry_len[0]==0 && issued<5)begin request_tx(0,8960);issued++;end
    else if(cycle-case_start>30000)begin
   if(tx_count[0]!=4*8960)$fatal(1,"one acknowledged segment opened %0d bytes, expected four segments in total",tx_count[0]);
   auto_ack=1;send_packet(0,peer_seq[0],server_seq[0]+4*8960,16);case_start=cycle;case_phase=3;
  end
  3:if(tx_count[0]==5*8960)case_done=1;else if(cycle-case_start>60000)$fatal(1,"the cumulative ACK did not release the fifth segment (%0d)",tx_count[0]);
 endcase
endtask
