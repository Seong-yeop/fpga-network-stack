// A fast retransmission after three duplicate ACKs reads exactly the missing segment; the cumulative ACK ends the recovery.
logic [71:0] rt_reads[$];bit capture=0;int issued=0;
task automatic case_init;
 auto_ack=0;retransmit_armed=1;
endtask
always @(posedge ap_clk)if(ap_rst_n && capture && m_axis_txread_cmd_TVALID && m_axis_txread_cmd_TREADY)rt_reads.push_back(m_axis_txread_cmd_TDATA);
task automatic case_step;
 if(connected[0] && cycle>45000)case(case_phase)
  0:if(!tx_pending[0])begin if(issued<7)begin request_tx(0,8960);issued++;end else if(tx_count[0]==7*8960)begin case_start=cycle;case_phase=1;end end
  1:if(cycle-case_start>200)begin
   send_packet(0,peer_seq[0],server_seq[0]+8960,16);capture=1;
   for(int i=0;i<3;i++)send_packet(0,peer_seq[0],server_seq[0]+8960,16);
   case_start=cycle;case_phase=2;
  end
  2:if(rt_reads.size()>=1)begin
   if(rt_reads[0][63:32]!=((server_seq[0]+8960)&32'h3FFFF))$fatal(1,"retransmission read addr %0h expected %0h",rt_reads[0][63:32],(server_seq[0]+8960)&32'h3FFFF);
   if(rt_reads[0][22:0]!=8960)$fatal(1,"retransmission read length %0d",rt_reads[0][22:0]);
   case_start=cycle;case_phase=3;
  end else if(cycle-case_start>20000)$fatal(1,"no retransmission read after three duplicate ACKs");
  3:if(cycle-case_start>3000)begin
   if(rt_reads.size()!=1)$fatal(1,"fast retransmission read %0d segments, expected 1",rt_reads.size());
   auto_ack=1;send_packet(0,peer_seq[0],server_seq[0]+7*8960,16);case_start=cycle;case_phase=4;
  end
  4:if(cycle-case_start>3000)begin
   if(rt_reads.size()!=1)$fatal(1,"%0d buffer reads after the cumulative ACK",rt_reads.size()-1);
   capture=0;if(!tx_pending[0])begin request_tx(0,64);case_phase=5;end
  end
  5:if(tx_count[0]==7*8960+64)case_done=1;
 endcase
endtask
