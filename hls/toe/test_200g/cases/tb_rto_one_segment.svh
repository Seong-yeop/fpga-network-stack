logic [71:0] rt_reads[$];bit capture=0;int issued=0;localparam int WARM=6*8960;
task automatic case_init;
 auto_ack=1;retransmit_armed=1;max_cycles=4000000;
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
  3:if(cycle-case_start>200)begin capture=1;case_start=cycle;case_phase=4;end
  4:if(rt_reads.size()>=1)begin
   if(cycle-case_start<300000)$fatal(1,"first RTO after %0d cycles is too early",cycle-case_start);
   expect_read(0,server_seq[0]+WARM,"first RTO retransmission");case_start=cycle;case_phase=5;
  end else if(cycle-case_start>800000)$fatal(1,"first RTO never fired");
  5:if(rt_reads.size()>=2)begin
   if(cycle-case_start<600000)$fatal(1,"second RTO after %0d cycles did not double",cycle-case_start);
   expect_read(1,server_seq[0]+WARM,"second RTO retransmission");
   send_packet(0,peer_seq[0],server_seq[0]+WARM+8960,16);case_start=cycle;case_phase=6;
  end else if(cycle-case_start>1500000)$fatal(1,"doubled RTO never fired");
  6:if(rt_reads.size()>=3)begin expect_read(2,server_seq[0]+WARM+8960,"partial ACK retransmission");send_packet(0,peer_seq[0],server_seq[0]+WARM+2*8960,16);case_start=cycle;case_phase=7;end
   else if(cycle-case_start>30000)$fatal(1,"partial ACK did not resend the next segment");
  7:if(rt_reads.size()>=4)begin expect_read(3,server_seq[0]+WARM+2*8960,"second partial ACK retransmission");send_packet(0,peer_seq[0],server_seq[0]+WARM+7*8960,16);case_start=cycle;case_phase=8;end
   else if(cycle-case_start>30000)$fatal(1,"second partial ACK did not resend the next segment");
  8:if(cycle-case_start>6000)begin
   if(rt_reads.size()!=4)$fatal(1,"%0d reads after the cumulative ACK, expected 4",rt_reads.size());
   capture=0;if(!tx_pending[0] && retry_len[0]==0)begin request_tx(0,8960);case_start=cycle;case_phase=9;end
  end
  9:if(!tx_pending[0] && retry_len[0]==0 && tx_count[0]==WARM+8*8960)begin request_tx(0,8960);case_start=cycle;case_phase=10;end
   else if(cycle-case_start>30000)$fatal(1,"one segment must fit the collapsed window (tx_count %0d)",tx_count[0]);
  10:if(cycle-case_start>30000)begin
   if(tx_count[0]!=WARM+8*8960)$fatal(1,"second segment left with a one-segment window (tx_count %0d)",tx_count[0]);
   if(tx_retries==0)$fatal(1,"the second segment was never rejected");
   send_packet(0,peer_seq[0],server_seq[0]+WARM+8*8960,16);auto_ack=1;case_start=cycle;case_phase=11;
  end
  11:if(tx_count[0]==WARM+9*8960)case_done=1;else if(cycle-case_start>60000)$fatal(1,"the ACK did not reopen the window (tx_count %0d)",tx_count[0]);
 endcase
endtask
