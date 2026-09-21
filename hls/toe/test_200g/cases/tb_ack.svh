task automatic case_init;auto_ack=0;endtask
task automatic case_step;
 if(connected[0] && cycle>45000)case(case_phase)
  0:begin request_tx(0,129);case_phase=1;end
  1:if(tx_count[0]==129)begin
   send_packet(0,peer_seq[0],server_seq[0]+10000,16);
   send_packet(0,peer_seq[0],server_seq[0]-1,16);
   for(int i=1;i<=129;i+=32)send_packet(0,peer_seq[0],server_seq[0]+i,16);
   send_packet(0,peer_seq[0],server_seq[0]+129,16);
   case_start=cycle;case_phase=2;
  end
  2:if(cycle-case_start>1000)begin auto_ack=1;request_tx(0,256);send_good(0,129);case_phase=3;end
  3:if(tx_count[0]==385 && rx_count[0]==129)case_done=1;
 endcase
endtask
