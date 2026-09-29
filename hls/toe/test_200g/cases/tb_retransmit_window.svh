logic [71:0] rt_reads[$];
bit capture=0;
int issued=0;
localparam int WARM=6*8960;
task automatic case_init;
 auto_ack=1;retransmit_armed=1;
endtask
always @(posedge ap_clk)if(ap_rst_n && capture && m_axis_txread_cmd_TVALID && m_axis_txread_cmd_TREADY)rt_reads.push_back(m_axis_txread_cmd_TDATA);
task automatic case_step;
 if(connected[0] && cycle>45000)case(case_phase)
  0:if(!tx_pending[0] && retry_len[0]==0 && tx_count[0]==issued*8960)begin
   if(issued<6)begin request_tx(0,8960);issued++;end
   else begin case_start=cycle;case_phase=1;end
  end
  1:if(cycle-case_start>3000)begin auto_ack=0;issued=0;case_phase=2;end
  2:if(!tx_pending[0] && retry_len[0]==0)begin
   if(issued<6)begin request_tx(0,8960);issued++;end
   else if(tx_count[0]==WARM+6*8960)begin case_start=cycle;case_phase=3;end
  end
  3:if(cycle-case_start>200)begin
   send_packet(0,peer_seq[0],server_seq[0]+WARM+8960,16);capture=1;
   for(int i=0;i<3;i++)send_packet(0,peer_seq[0],server_seq[0]+WARM+8960,16);
   case_start=cycle;case_phase=4;
  end
  4:if(cycle-case_start>10000)begin
   int unsigned bytes_read;
   bytes_read=0;
   foreach(rt_reads[i])begin
    if(rt_reads[i][63:32]!=((server_seq[0]+WARM+8960+bytes_read)&32'h3FFFF))$fatal(1,"original retransmission read address");
    if(rt_reads[i][22:0]==0||rt_reads[i][22:0]>8960)$fatal(1,"original retransmission read length");
    bytes_read+=rt_reads[i][22:0];
   end
   if(bytes_read!=4*8960)$fatal(1,"original retransmission read %0d bytes, expected four MSS",bytes_read);
   case_packets=rt_reads.size();auto_ack=1;
   send_packet(0,peer_seq[0],server_seq[0]+WARM+6*8960,16);
   case_start=cycle;case_phase=5;
  end
  5:if(cycle-case_start>3000)begin
   if(rt_reads.size()!=case_packets)$fatal(1,"buffer read after the cumulative ACK");
   capture=0;if(!tx_pending[0])begin request_tx(0,64);case_phase=6;end
  end
  6:if(tx_count[0]==WARM+6*8960+64)case_done=1;
 endcase
endtask
