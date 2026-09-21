int unsigned original_isn;
task automatic case_init;auto_handshake=0;endtask
task automatic case_step;
 if(connected[0] && cycle>45000)case(case_phase)
  0:begin original_isn=server_seq[0];case_packets=packet_count[0];send_packet(0,peer_isn[0],0,2,0,1);case_phase=1;end
  1:if(packet_count[0]>case_packets)begin
   if(server_seq[0]!=original_isn)$fatal(1,"duplicate SYN changed ISN");auto_handshake=1;
   send_packet(0,peer_seq[0],server_seq[0],16);case_start=cycle;case_phase=2;
  end
  2:if(cycle-case_start>500)begin send_packet(0,peer_seq[0]+10,server_seq[0],4);case_start=cycle;case_phase=3;end
  3:if(cycle-case_start>500)begin send_good(0,65);case_phase=4;end
  4:if(rx_count[0]==65)begin send_packet(0,peer_seq[0],server_seq[0],4);case_start=cycle;case_phase=5;end
  5:if(cycle-case_start>500)begin send_packet(0,peer_seq[0],server_seq[0],24,64);case_start=cycle;case_phase=6;end
  6:if(cycle-case_start>1000)begin if(rx_count[0]!=65)$fatal(1,"data delivered after reset");case_done=1;end
 endcase
endtask
