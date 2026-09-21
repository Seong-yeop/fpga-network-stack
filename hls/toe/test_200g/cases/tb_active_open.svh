task automatic case_init;active_open=1;endtask
task automatic case_step;
 if(connected[0] && cycle>45000)case(case_phase)
  0:begin send_good(0,129);request_tx(0,128);case_phase=1;end
  1:if(rx_count[0]==129 && tx_count[0]==128)case_done=1;
 endcase
endtask
