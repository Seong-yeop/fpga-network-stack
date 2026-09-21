task automatic case_init;endtask
task automatic case_step;
 if(connected[0] && cycle>45000)case(case_phase)
  0:begin send_good(0,128);case_phase=1;end
  1:if(rx_count[0]==128)begin
   send_packet(0,peer_seq[0]+128,server_seq[0],24,128);
   send_packet(0,peer_seq[0]-128,server_seq[0],24,128);
   send_packet(0,peer_seq[0]-64,server_seq[0],24,128);
   case_start=cycle;case_phase=2;
  end
  2:if(cycle-case_start>2000)begin
   if(rx_count[0]!=128)$fatal(1,"duplicate or out-of-order bytes delivered");
   send_good(0,128);send_good(0,128);case_phase=3;
  end
  3:if(rx_count[0]==384)case_done=1;
 endcase
endtask
