// One retransmission event must cover the whole unacked window (six segments), not four.
logic [71:0] rt_reads[$];bit capture=0;int issued=0;int unsigned base_addr=0;
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
  2:if(rt_reads.size()>=6)begin
   base_addr=rt_reads[0][63:32];
   for(int k=0;k<6;k++)begin
    if(rt_reads[k][63:32]!=base_addr+k*8960)$fatal(1,"retransmission read %0d addr %0h expected %0h",k,rt_reads[k][63:32],base_addr+k*8960);
    if(rt_reads[k][22:0]!=8960)$fatal(1,"retransmission read %0d length %0d",k,rt_reads[k][22:0]);
   end
   capture=0;auto_ack=1;send_packet(0,peer_seq[0],server_seq[0]+8960,16);case_start=cycle;case_phase=3;
  end else if(cycle-case_start>20000)$fatal(1,"retransmission issued only %0d reads",rt_reads.size());
  3:if(cycle-case_start>6000 && !tx_pending[0])begin request_tx(0,64);case_phase=4;end
  4:if(tx_count[0]==7*8960+64)case_done=1;
 endcase
endtask
