task automatic case_init;active_open=1;auto_ack=0;endtask
task automatic case_step;
 if(connected[0] && cycle>45000)case(case_phase)
  0:begin
   send_good(0,128);send_good(0,129);send_good(0,8960);
   request_tx(0,128);request_tx(0,129);request_tx(0,8960);case_phase=1;
  end
  1:if(tx_count[0]==9217)begin
   hold_acks=1;retransmit_seq=server_seq[0];retransmit_armed=1;auto_ack=1;
   for(int i=0;i<4;i++)send_packet(0,peer_seq[0],server_seq[0],16);
   case_phase=2;
  end
  2:if(rx_count[0]==9217 && saw_retransmit && read_cmds.size()==0 && s_axis_txread_data_q.size()==0)begin case_start=cycle;case_phase=3;end
  3:if(cycle-case_start>3000)case_done=1;
 endcase
endtask
