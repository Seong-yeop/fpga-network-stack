bit reset_seen=0;
task automatic case_init;endtask
task automatic case_step;
 if(cycle==39000)send_packet(0,peer_isn[0],0,2,0,1);
 if(cycle==39900)begin if(!(last_flags[0]&4))$fatal(1,"closed port SYN not reset");reset_seen=1;end
 if(connected[0] && cycle>45000)case(case_phase)
  0:begin send_good(0,65);case_phase=1;end
  1:if(rx_count[0]==65 && reset_seen)case_done=1;
 endcase
endtask
