// Shared setup/clock only. The selected cases/tb_*.svh defines case_step().
int case_phase=0,case_start=0,case_packets=0;
bit case_done=0,automatic_syn=1;
function automatic bit permit(input int index,input bit source);
 if(target>=0 && !source && index!=target)return 1;
 case(profile)
  1:return (cycle+index)%2!=0;
  2:return (cycle+index)%4==3;
  3:return (cycle+index)%4!=3;
  4:return ((random_state>>(index%24))&3)==3;
  5:return ((random_state>>(index%24))&1)!=0;
  6:return ((random_state>>(index%24))&3)!=0;
  7:return !(cycle>=45100 && cycle<46124);
  default:return 1;
 endcase
endfunction
task automatic case_setup;
 s_axis_listen_port_req_word lp;s_axis_open_conn_req_word op;
 if(cycle==40000)begin lp='0;lp.TDATA=5001;s_axis_listen_port_req_q.push_back(lp);end
 if(cycle==42000)begin
  if(active_open)begin op='0;op.TDATA={16'd40000,32'h0a000002};s_axis_open_conn_req_q.push_back(op);end
  else if(automatic_syn)for(int i=0;i<n_sessions;i++)send_packet(i,peer_isn[i],0,2,0,1,peer_ws);
 end
 if(delayed_acks.size() && delayed_acks[0].due<=cycle)begin
  ack_t a;a=delayed_acks.pop_front();send_packet(a.sid,peer_seq[a.sid],a.ack,16);
 end
 for(int i=0;i<n_sessions;i++)if(retry_len[i]!=0 && cycle>=retry_at[i] && !tx_pending[i])begin
  int n;n=retry_len[i];retry_len[i]=0;request_tx(i,n);
 end
endtask
task automatic send_good(input int sid,input int n);
 send_packet(sid,peer_seq[sid],server_seq[sid]+tx_count[sid],24,n);peer_seq[sid]+=n;
endtask
initial begin ap_clk=0;forever #2 ap_clk=~ap_clk;end
initial begin
 mode="case";n_sessions=1;packet_log=$fopen("packets.hex","w");
 if(!$value$plusargs("SEED=%d",seed))seed=1;
 if(!$value$plusargs("PROFILE=%d",profile))profile=0;
 if(!$value$plusargs("TARGET=%d",target))target=-1;
 if(!$value$plusargs("STALL=%d",stall_cycles))stall_cycles=0;
 if(!$value$plusargs("POSITION=%d",position))position=0;
 if(target < -1 || target >= NUM_OUTPUTS || profile<0 || profile>7 || position<0 || position>2 || stall_cycles<0)$fatal(1,"invalid stall configuration");
 random_state=seed?seed:1;
 for(int i=0;i<8;i++)begin peer_seq[i]=1001;acked_peer[i]=1001;advertised[i]=0;server_seq[i]=0;tx_count[i]=0;rx_count[i]=0;tx_requested[i]=0;connected[i]=0;tx_pending[i]=0;retry_len[i]=0;retry_at[i]=0;last_flags[i]=0;packet_count[i]=0;end
 init_stalls();case_init();
 ap_rst_n=0;myIpAddress='h0100000a;axis_data_count=0;axis_max_data_count=8192;
 drive_inputs();repeat(10)@(negedge ap_clk);ap_rst_n=1;
 forever begin
  case_setup();case_step();drive_inputs();
  axis_data_count=rx_fifo.size()+s_axis_rxread_data_q.size();
  m_axis_rxwrite_data_TREADY=axis_data_count<8192;
  m_axis_rx_data_rsp_TREADY=app_fifo.size()<8;
  m_axis_txwrite_data_TREADY=write_cmds.size()!=0;
  m_axis_txwrite_cmd_TREADY=write_cmds.size()<8;
  m_axis_txread_cmd_TREADY=read_cmds.size()<8;
  m_axis_session_lup_req_TREADY=s_axis_session_lup_rsp_q.size()<4;
  m_axis_session_upd_req_TREADY=s_axis_session_upd_rsp_q.size()<4;
  apply_stalls();
  @(posedge ap_clk);observe_stalls();pop_inputs();service_models();cycle++;
  if(case_done)begin
   check_coverage();$fclose(packet_log);$display("PASS %s width=%0d seed=%0d profile=%0d target=%0d stall=%0d",CASE_NAME,W,seed,profile,target,stall_cycles);$finish;
  end
  if(cycle>=200000)begin report_coverage();$fatal(1,"case timeout %s phase=%0d rx=%0d tx=%0d",CASE_NAME,case_phase,rx_bytes,tx_bytes);end
  @(negedge ap_clk);random_state=(random_state>>1)^((0-(random_state&1))&32'hd0000001);
 end
end
