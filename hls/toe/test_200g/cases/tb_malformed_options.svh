task automatic case_init;automatic_syn=0;expect_ws=0;n_sessions=2;endtask
task automatic case_step;
 if(cycle==42000)send_packet(0,peer_isn[0],0,2,0,1,3,0,0,0);
 case(case_phase)
  0:if(connected[0] && cycle>45000)begin send_good(0,129);case_phase=1;end
  1:if(rx_count[0]==129)begin expect_ws=1;send_packet(1,peer_isn[1],0,2,0,1);case_phase=2;end
  2:if(connected[1])begin send_good(1,65);case_phase=3;end
  3:if(rx_count[1]==65)case_done=1;
 endcase
endtask
