logic [71:0] rt_reads[$];bit capture=0;int issued=0;localparam int WARM=6*8960;
task automatic case_init;
 auto_ack=1;retransmit_armed=1;max_cycles=2500000;
endtask
always @(posedge ap_clk)if(ap_rst_n && capture && m_axis_txread_cmd_TVALID && m_axis_txread_cmd_TREADY)rt_reads.push_back(m_axis_txread_cmd_TDATA);
task automatic expect_read(input int index,input int unsigned seq,input string why);
 if(rt_reads.size()<=index)$fatal(1,"%s: only %0d reads",why,rt_reads.size());
 if(rt_reads[index][63:32]!=(seq&32'h3FFFF))$fatal(1,"%s: read addr %0h expected %0h",why,rt_reads[index][63:32],seq&32'h3FFFF);
 if(rt_reads[index][22:0]!=8960)$fatal(1,"%s: read length %0d",why,rt_reads[index][22:0]);
endtask
task automatic case_step;
 if(connected[0] && cycle>45000)case(case_phase)
  0:if(!tx_pending[0] && retry_len[0]==0)begin if(issued<6)begin request_tx(0,8960);issued++;end else if(tx_count[0]==WARM)begin case_start=cycle;case_phase=1;end end
  1:if(cycle-case_start>4000)begin auto_ack=0;case_phase=2;end
  2:if(!tx_pending[0] && retry_len[0]==0)begin if(issued<13)begin request_tx(0,8960);issued++;end else if(tx_count[0]==WARM+7*8960)begin case_start=cycle;case_phase=3;end end
  3:if(cycle-case_start>200)begin capture=1;send_packet(0,peer_seq[0],server_seq[0]+WARM+6*8960,16);case_start=cycle;case_phase=4;end
  4:if(rt_reads.size()>=1)begin
   if(cycle-case_start<300000)$fatal(1,"tail retransmission after %0d cycles is earlier than the RTO",cycle-case_start);
   expect_read(0,server_seq[0]+WARM+6*8960,"tail retransmission");
   auto_ack=1;send_packet(0,peer_seq[0],server_seq[0]+WARM+7*8960,16);case_start=cycle;case_phase=5;
  end else if(cycle-case_start>800000)$fatal(1,"RTO did not resend the lost tail segment");
  5:if(cycle-case_start>6000)begin
   if(rt_reads.size()!=1)$fatal(1,"%0d reads after the full ACK, expected 1",rt_reads.size());
   capture=0;if(!tx_pending[0])begin request_tx(0,64);case_phase=6;end
  end
  6:if(tx_count[0]==WARM+7*8960+64)case_done=1;
 endcase
endtask
