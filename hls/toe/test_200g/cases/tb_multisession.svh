task automatic case_init;n_sessions=8;endtask
task automatic case_step;
 bit ready_all;ready_all=1;for(int i=0;i<8;i++)ready_all&=connected[i];
 if(ready_all && cycle>47000)case(case_phase)
  0:begin for(int i=0;i<8;i++)begin send_good(i,65+i);request_tx(i,127+i);end case_phase=1;end
  1:begin
   bit complete;complete=1;for(int i=0;i<8;i++)complete&=(rx_count[i]==65+i && tx_count[i]==127+i);
   if(complete)case_done=1;
  end
 endcase
endtask
