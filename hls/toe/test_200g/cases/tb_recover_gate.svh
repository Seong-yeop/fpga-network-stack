logic [71:0] rt_reads[$];bit capture=0;int issued=0;localparam int WARM=6*8960;
task automatic case_init;
 auto_ack=1;retransmit_armed=1;max_cycles=1500000;
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
  2:if(!tx_pending[0] && retry_len[0]==0)begin if(issued<12)begin request_tx(0,8960);issued++;end else if(tx_count[0]==WARM+6*8960)begin case_start=cycle;case_phase=3;end end
  3:if(cycle-case_start>200)begin capture=1;send_packet(0,peer_seq[0],server_seq[0]+WARM,16);for(int i=0;i<3;i++)send_packet(0,peer_seq[0],server_seq[0]+WARM,16);case_start=cycle;case_phase=4;end
  4:if(rt_reads.size()>=1)begin expect_read(0,server_seq[0]+WARM,"first fast retransmission");send_packet(0,peer_seq[0],server_seq[0]+WARM+5*8960,16);case_start=cycle;case_phase=5;end
   else if(cycle-case_start>20000)$fatal(1,"no fast retransmission after three duplicate ACKs");
  5:if(rt_reads.size()>=2)begin expect_read(1,server_seq[0]+WARM+5*8960,"partial ACK retransmission");send_packet(0,peer_seq[0],server_seq[0]+WARM+6*8960,16);case_start=cycle;case_phase=6;end
   else if(cycle-case_start>20000)$fatal(1,"partial ACK did not resend the last hole");
  6:if(cycle-case_start>3000)begin if(rt_reads.size()!=2)$fatal(1,"read after the recovery exit");issued=0;case_phase=7;end
  7:if(!tx_pending[0] && retry_len[0]==0)begin if(issued<2)begin request_tx(0,8960);issued++;end else if(tx_count[0]==WARM+8*8960)begin case_start=cycle;case_phase=8;end end
  8:if(cycle-case_start>200)begin for(int i=0;i<4;i++)send_packet(0,peer_seq[0],server_seq[0]+WARM+6*8960,16);case_start=cycle;case_phase=9;end
  9:if(cycle-case_start>20000)begin
   if(rt_reads.size()!=2)$fatal(1,"duplicate ACKs at recover started another fast retransmit (%0d reads)",rt_reads.size());
   send_packet(0,peer_seq[0],server_seq[0]+WARM+7*8960,16);for(int i=0;i<3;i++)send_packet(0,peer_seq[0],server_seq[0]+WARM+7*8960,16);case_start=cycle;case_phase=10;
  end
  10:if(rt_reads.size()>=3)begin expect_read(2,server_seq[0]+WARM+7*8960,"fast retransmission beyond recover");auto_ack=1;send_packet(0,peer_seq[0],server_seq[0]+WARM+8*8960,16);case_start=cycle;case_phase=11;end
   else if(cycle-case_start>20000)$fatal(1,"duplicate ACKs beyond recover did not fast-retransmit");
  11:if(cycle-case_start>6000)begin if(rt_reads.size()!=3)$fatal(1,"%0d reads after the final ACK, expected 3",rt_reads.size());case_done=1;end
 endcase
endtask
