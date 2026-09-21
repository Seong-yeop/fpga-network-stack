`include "common/xsim.svh"
int lengths[10]='{1,63,64,65,127,128,129,8959,8960,8961};
int functional_index=0,send_phase=0;
int unsigned sent_rx[8];
task automatic scenario;
 bit all_connected;int sid,len;ack_t a;window_t update;
 s_axis_listen_port_req_word lp;
 for(int i=0;i<n_sessions;i++)if(retry_len[i]!=0 && cycle>=retry_at[i] && !tx_pending[i])begin len=retry_len[i];retry_len[i]=0;request_tx(i,len);end
 if(mode=="functional" && cycle==61000)begin peer_window=0;send_packet(0,peer_seq[0],server_seq[0],16);end
 if(mode=="functional" && cycle>61500 && cycle<65000 && tx_bytes!=0)$fatal(1,"TX ignored zero window");
 if(mode=="functional" && cycle==65000)begin peer_window=65535;send_packet(0,peer_seq[0],server_seq[0],16);saw_zero=1;end
 if(cycle==40000)begin lp='0;lp.TDATA=5001;s_axis_listen_port_req_q.push_back(lp);end
 for(int i=0;i<n_sessions;i++)if(cycle==42000+i*2000)send_packet(i,1000,0,2,0,1);
 if(delayed_windows.size() && delayed_windows[0].due<=cycle)begin
  update=delayed_windows.pop_front();
  if(update.window==0)begin zero_windows++;saw_rx_zero[update.sid]=1;end else if(advertised[update.sid]==0 && saw_rx_zero[update.sid])window_reopens++;
  advertised[update.sid]=update.window;acked_peer[update.sid]=update.ack;
 end
 if(delayed_acks.size() && delayed_acks[0].due<=cycle)begin a=delayed_acks.pop_front();send_packet(a.sid,peer_seq[a.sid],a.ack,16);end
 all_connected=1;for(int i=0;i<n_sessions;i++)all_connected &= connected[i];
 if((mode=="flow" || mode=="full") && cycle==125000)begin recovery=1;bytes_at_resume=rx_bytes;end
 if(recovery)begin
  bit caught_up; caught_up=1;
  for(int i=0;i<n_sessions;i++)if(acked_peer[i]<peer_seq[i])begin
   caught_up=0;
   if(cycle>=next_recovery[i])begin len=peer_seq[i]-acked_peer[i];if(len>8960)len=8960;send_packet(i,acked_peer[i],server_seq[i]+tx_count[i],24,len);next_recovery[i]=cycle+2500;end
  end
  if(caught_up)recovery=0;
 end
 if(cycle>60000 && all_connected)begin
  if(mode=="functional")begin
   if(functional_index<42 && cycle%3000==0 && !tx_pending[0] && retry_len[0]==0)begin
    len=(functional_index<10)?lengths[functional_index]:8960;send_packet(0,peer_seq[0],server_seq[0]+tx_count[0],24,len);peer_seq[0]+=len;sent_rx[0]+=len;request_tx(0,len);tx_goal+=len;functional_index++;
   end
   if(functional_index==42 && cycle>190000 && rx_count[0]==sent_rx[0] && tx_count[0]==tx_goal)begin
    if(!saw_retransmit)$fatal(1,"retransmission not exercised");
    if(!saw_zero || tx_retries==0)$fatal(1,"zero window not exercised");
    $display("PASS functional width=%0d frequency=250MHz rx=%0d tx=%0d peak_fifo=%0d",W,rx_bytes,tx_bytes,peak_fifo);$finish;
   end
  end else begin
   if(mode!="tx" && s_axis_tcp_data_q.size()<2 && !recovery && (mode!="full" || cycle<112000))begin
    sid=round_rx;
    if(mode=="flow" && advertised[sid]==0 && cycle>=next_probe[sid])begin send_packet(sid,peer_seq[sid]-1,server_seq[sid]+tx_count[sid],16,1);next_probe[sid]=cycle+2500;end
    if(mode=="full" || peer_seq[sid]-acked_peer[sid]+8960<=advertised[sid])begin send_packet(sid,peer_seq[sid],server_seq[sid]+tx_count[sid],24,8960);peer_seq[sid]+=8960;sent_rx[sid]+=8960;end
    round_rx=(round_rx+1)%n_sessions;
   end
   if(mode!="rx" && mode!="flow" && mode!="full" && s_axis_tx_data_req_q.size()<2)begin
    sid=round_tx;if(!tx_pending[sid] && retry_len[sid]==0 && tx_requested[sid]-tx_count[sid]<53760)request_tx(sid,8960);round_tx=(round_tx+1)%n_sessions;
   end
   if(cycle==100000)begin measure_start=cycle;rx_start=rx_bytes;tx_start=tx_bytes;measured=1;end
   if(measured && cycle==measure_start+250000)begin
    $display("RESULT mode=%s width=%0d period_ns=4 rx_Gbps=%f tx_Gbps=%f peak_fifo=%0d sessions=%0d",mode,W,(rx_bytes-rx_start)*8.0/1000000.0,(tx_bytes-tx_start)*8.0/1000000.0,peak_fifo,n_sessions);
    if(n_sessions==8 && mode!="flow" && mode!="full" && ((mode!="tx" && (rx_bytes-rx_start)<25000000)||(mode!="rx" && (tx_bytes-tx_start)<25000000)))$fatal(1,"200Gbps payload target missed");
    if(mode=="full")begin
     if(peak_fifo<8192-375 || peak_fifo>8192 || rx_fifo.size()!=0)$fatal(1,"RX near-full guard not exercised/drained");
     for(int i=0;i<n_sessions;i++)if(rx_count[i]!=sent_rx[i])$fatal(1,"full FIFO data loss");
    end
    if(mode=="flow" && (zero_windows==0 || window_reopens==0 || peak_fifo<6144 || rx_bytes<=bytes_at_resume))$fatal(1,"RX zero-window not exercised");
    $display("PASS performance %s zero_windows=%0d reopens=%0d",mode,zero_windows,window_reopens);$finish;
   end
  end
 end
 if(cycle==400000)$fatal(1,"timeout connected=%p rx=%0d tx=%0d txq=%0d meta=%0d wc=%0d wo=%0d",connected,rx_bytes,tx_bytes,s_axis_tx_data_req_q.size(),s_axis_tx_data_req_metadata_q.size(),write_cmds.size(),write_offset);
endtask
initial begin
 ap_clk=0;forever #2 ap_clk=~ap_clk;
end
initial begin
 if(!$value$plusargs("MODE=%s",mode))mode="functional";
 if(!(mode inside {"functional","rx","tx","duplex","flow","full"}))$fatal(1,"invalid mode");
 if(!$value$plusargs("SESSIONS=%d",n_sessions))n_sessions=8;
 if(n_sessions!=1 && n_sessions!=8)$fatal(1,"SESSIONS must be 1 or 8");
 if(mode=="functional")n_sessions=1;
 for(int i=0;i<8;i++)begin peer_seq[i]=1001;acked_peer[i]=1001;advertised[i]=0;server_seq[i]=0;tx_count[i]=0;rx_count[i]=0;tx_requested[i]=0;sent_rx[i]=0;connected[i]=0;tx_pending[i]=0;retry_len[i]=0;retry_at[i]=0;saw_rx_zero[i]=0;next_probe[i]=0;next_recovery[i]=0;end
 ap_rst_n=0;myIpAddress='h0100000a;axis_data_count=0;axis_max_data_count=8192;
 drive_inputs();repeat(10)@(negedge ap_clk);ap_rst_n=1;
 forever begin
  scenario();drive_inputs();
  axis_data_count=rx_fifo.size()+s_axis_rxread_data_q.size();
  m_axis_rxwrite_data_TREADY=rx_fifo.size()+s_axis_rxread_data_q.size()<8192;
  m_axis_rx_data_rsp_TREADY=app_fifo.size()<8;
  if(mode=="functional")begin
   random_state=(random_state>>1)^((0-(random_state&1))&32'hd0000001);
   m_axis_tcp_data_TREADY=random_state[0]|random_state[1];
   m_axis_rx_data_rsp_TREADY=(random_state[2]|random_state[3]) && app_fifo.size()<8;
   m_axis_txwrite_data_TREADY=(random_state[4]|random_state[5]) && write_cmds.size()!=0;
  end else m_axis_txwrite_data_TREADY=write_cmds.size()!=0;
  if((mode=="flow" || mode=="full") && cycle>=100000 && cycle<125000)m_axis_rx_data_rsp_TREADY=0;
  @(posedge ap_clk);pop_inputs();service_models();cycle++;
  @(negedge ap_clk);
 end
end
