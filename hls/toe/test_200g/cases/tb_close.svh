task automatic case_init;endtask
task automatic case_step;
 s_axis_close_conn_req_word c;
 if(connected[0] && cycle>45000)case(case_phase)
  0:begin send_good(0,65);case_phase=1;end
  1:if(rx_count[0]==65)begin c='0;c.TDATA=peer_to_session[0];s_axis_close_conn_req_q.push_back(c);case_phase=2;end
  2:if(last_flags[0]&1)begin send_packet(0,peer_seq[0],last_seq[0]+1,17,129);peer_seq[0]+=130;case_phase=3;end
  3:if(rx_count[0]==194 && last_ack[0]==peer_seq[0])case_done=1;
 endcase
endtask
