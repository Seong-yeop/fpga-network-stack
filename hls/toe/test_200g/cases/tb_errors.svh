task automatic case_init;endtask
task automatic case_step;
 if(connected[0] && cycle>45000)case(case_phase)
  0:begin send_packet(0,peer_seq[0],server_seq[0],24,65,0,3,1);case_start=cycle;case_phase=1;end
  1:if(cycle-case_start>1000)begin
   if(rx_count[0]!=0)$fatal(1,"bad checksum delivered");send_good(0,65);case_phase=2;
  end
  2:if(rx_count[0]==65)begin
   send_packet(0,peer_seq[0],server_seq[0],24,65,0,3,0,4);
   send_packet(0,peer_seq[0],server_seq[0],16,0,0,3,0,15);
   case_start=cycle;case_phase=3;
  end
  3:if(cycle-case_start>1000)begin
   if(rx_count[0]!=65)$fatal(1,"invalid data offset delivered");send_good(0,129);case_phase=4;
  end
  4:if(rx_count[0]==194)case_done=1;
 endcase
endtask
