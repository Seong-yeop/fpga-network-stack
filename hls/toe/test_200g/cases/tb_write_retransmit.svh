// No read may be issued before the corresponding TX buffer write completes.
bit write_done=0,read_seen=0;
task automatic case_init;
 auto_ack=0;target=4;position=0;if(stall_cycles==0)stall_cycles=2000;
endtask
always @(posedge ap_clk)if(ap_rst_n)begin
 if(s_axis_txwrite_sts_TVALID && s_axis_txwrite_sts_TREADY)write_done<=1;
 if(m_axis_txread_cmd_TVALID && m_axis_txread_cmd_TREADY)begin
  if(!write_done)$fatal(1,"retransmission read before write status");read_seen<=1;
 end
end
task automatic case_step;
 if(connected[0] && cycle>45000)case(case_phase)
  0:begin request_tx(0,8960);case_phase=1;end
  1:if(tx_count[0]==8960)begin
   hold_acks=1;retransmit_seq=server_seq[0];retransmit_armed=1;auto_ack=1;
   for(int i=0;i<4;i++)send_packet(0,peer_seq[0],server_seq[0],16);
   case_phase=2;
  end
  2:if(write_done && read_seen && saw_retransmit && read_cmds.size()==0)begin case_start=cycle;case_phase=3;end
  3:if(cycle-case_start>3000)case_done=1;
 endcase
endtask
