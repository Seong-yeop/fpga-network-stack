// Two holes in one window: the third duplicate ACK resends the first hole, the partial ACK resends the second at once.
logic [71:0] rt_reads[$];bit capture=0;int issued=0;
task automatic case_init;
 auto_ack=0;retransmit_armed=1;
endtask
always @(posedge ap_clk)if(ap_rst_n && capture && m_axis_txread_cmd_TVALID && m_axis_txread_cmd_TREADY)rt_reads.push_back(m_axis_txread_cmd_TDATA);
task automatic expect_read(input int index,input int unsigned seq,input string why);
 if(rt_reads[index][63:32]!=(seq&32'h3FFFF))$fatal(1,"%s: read addr %0h expected %0h",why,rt_reads[index][63:32],seq&32'h3FFFF);
 if(rt_reads[index][22:0]!=8960)$fatal(1,"%s: read length %0d",why,rt_reads[index][22:0]);
endtask
task automatic case_step;
 if(connected[0] && cycle>45000)case(case_phase)
  0:if(!tx_pending[0])begin if(issued<7)begin request_tx(0,8960);issued++;end else if(tx_count[0]==7*8960)begin case_start=cycle;case_phase=1;end end
  1:if(cycle-case_start>200)begin
   send_packet(0,peer_seq[0],server_seq[0]+8960,16);capture=1;
   for(int i=0;i<3;i++)send_packet(0,peer_seq[0],server_seq[0]+8960,16);
   case_start=cycle;case_phase=2;
  end
  2:if(rt_reads.size()>=1)begin expect_read(0,server_seq[0]+8960,"first hole");case_start=cycle;case_phase=3;end
   else if(cycle-case_start>20000)$fatal(1,"no retransmission read after three duplicate ACKs");
  3:if(cycle-case_start>2000)begin
   if(rt_reads.size()!=1)$fatal(1,"first trigger read %0d segments, expected 1",rt_reads.size());
   send_packet(0,peer_seq[0],server_seq[0]+4*8960,16);case_start=cycle;case_phase=4;
  end
  4:if(rt_reads.size()>=2)begin expect_read(1,server_seq[0]+4*8960,"second hole");case_start=cycle;case_phase=5;end
   else if(cycle-case_start>20000)$fatal(1,"partial ACK did not resend the second hole");
  5:if(cycle-case_start>2000)begin
   if(rt_reads.size()!=2)$fatal(1,"%0d reads after the partial ACK, expected 2",rt_reads.size());
   auto_ack=1;send_packet(0,peer_seq[0],server_seq[0]+7*8960,16);case_start=cycle;case_phase=6;
  end
  6:if(cycle-case_start>3000)begin
   if(rt_reads.size()!=2)$fatal(1,"%0d buffer reads after the full ACK",rt_reads.size()-2);
   capture=0;if(!tx_pending[0])begin request_tx(0,64);case_phase=7;end
  end
  7:if(tx_count[0]==7*8960+64)case_done=1;
 endcase
endtask
