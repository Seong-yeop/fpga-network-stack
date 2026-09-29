logic [71:0] rt_reads[$];bit capture=0;int issued=0,acks=0;int unsigned nxt=0;
task automatic case_init;
 auto_ack=0;retransmit_armed=1;max_cycles=6000000;
endtask
always @(posedge ap_clk)if(ap_rst_n && capture && m_axis_txread_cmd_TVALID && m_axis_txread_cmd_TREADY)rt_reads.push_back(m_axis_txread_cmd_TDATA);
task automatic case_step;
 if(connected[0] && cycle>45000)case(case_phase)
  0:if(!tx_pending[0] && retry_len[0]==0 && issued<2)begin request_tx(0,8960);issued++;end
    else if(tx_count[0]==issued*8960 && issued==2)begin case_start=cycle;case_phase=1;end
  1:if(cycle-case_start>200000)begin acks++;send_packet(0,peer_seq[0],server_seq[0]+acks*8960,16);case_start=cycle;if(acks==2)case_phase=2;end
  2:if(cycle-case_start>3000)begin capture=1;request_tx(0,8960);issued++;case_start=cycle;case_phase=3;end
  3:if(rt_reads.size()>=1)begin
   if(cycle-case_start<550000)$fatal(1,"RTO after %0d cycles ignored the measured round trips",cycle-case_start);
   if(cycle-case_start>1200000)$fatal(1,"RTO after %0d cycles is too long for the measured round trips",cycle-case_start);
   send_packet(0,peer_seq[0],server_seq[0]+3*8960,16);acks=3;case_start=cycle;case_phase=4;
  end else if(cycle-case_start>1500000)$fatal(1,"RTO never fired");
  4:if(!tx_pending[0] && retry_len[0]==0 && cycle-case_start>3000)begin if(issued<7)begin request_tx(0,8960);issued++;case_start=cycle;case_phase=5;end else begin capture=1;rt_reads.delete();request_tx(0,8960);issued++;case_start=cycle;case_phase=6;end end
  5:if(tx_count[0]==issued*8960 && cycle-case_start>20000)begin acks++;send_packet(0,peer_seq[0],server_seq[0]+acks*8960,16);case_start=cycle;case_phase=4;end
  6:if(rt_reads.size()>=1)begin
   if(cycle-case_start<300000)$fatal(1,"RTO after %0d cycles is below the floor",cycle-case_start);
   if(cycle-case_start>600000)$fatal(1,"RTO after %0d cycles did not fall back to the floor after fresh samples",cycle-case_start);
   auto_ack=1;send_packet(0,peer_seq[0],server_seq[0]+8*8960,16);case_start=cycle;case_phase=7;
  end else if(cycle-case_start>1500000)$fatal(1,"second RTO never fired");
  7:if(cycle-case_start>6000)begin capture=0;if(!tx_pending[0])begin request_tx(0,64);case_phase=8;end end
  8:if(tx_count[0]==8*8960+64)case_done=1;
 endcase
endtask
