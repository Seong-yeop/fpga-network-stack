task automatic case_init;peer_isn[0]=32'hfffffff0;peer_seq[0]=peer_isn[0]+1;endtask
task automatic case_step;
 if(connected[0] && cycle>45000)case(case_phase)
  0:begin send_good(0,129);send_good(0,8960);request_tx(0,129);case_phase=1;end
  1:if(rx_count[0]==9089 && tx_count[0]==129)case_done=1;
 endcase
endtask
