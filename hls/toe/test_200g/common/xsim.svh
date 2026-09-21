// Independent transaction-level environment around the synthesized TOE.
// Byte 0 occupies TDATA[7:0]. TCP/IP fields retain network byte order.
typedef byte unsigned bytes_t[$];
typedef struct packed {logic [W-1:0] data;logic [K-1:0] keep;logic last;} beat_t;
beat_t rx_fifo[$], app_fifo[$];
logic [71:0] write_cmds[$], read_cmds[$];
byte unsigned mem[longint unsigned];
logic [15:0] sessions[logic [63:0]];
bytes_t packet_out;int packet_log=0;
int cycle=0,write_offset=0,read_offset=0,peak_fifo=0;
int unsigned peer_seq[8],server_seq[8],tx_count[8],rx_count[8],tx_requested[8];
int unsigned advertised[8],acked_peer[8];
bit connected[8], tx_pending[8];
bit retransmit_armed=0,hold_acks=0,saw_retransmit=0;int unsigned retransmit_seq=0;
int tx_retries=0;int retry_len[8], retry_at[8];int unsigned tx_goal=0;
int app_sessions[$], app_lengths[$], expected_read_lengths[$];
int unsigned rx_model_session=0;
longint unsigned rx_bytes=0,tx_bytes=0,rx_start=0,tx_start=0;
int round_rx=0,round_tx=0,measure_start=0;
int unsigned peer_isn[8]='{default:1000};
int my_port[8]='{default:5001};
bit expect_ws=1,auto_ack=1,active_open=0,auto_handshake=1;
int peer_ws=3;int peer_to_session[8]='{0,1,2,3,4,5,6,7};
int last_flags[8],packet_count[8];int unsigned last_ack[8],last_seq[8];
int session_to_peer[int];
int n_sessions=8;int peer_window=65535;bit saw_zero=0;int zero_windows=0,window_reopens=0;
string mode="functional";
bit measured=0;
int unsigned random_state=32'h12345678;
typedef struct {int due; int sid; int unsigned ack;} ack_t;
ack_t delayed_acks[$];
typedef struct {int due;int sid;int unsigned ack;int unsigned window;} window_t;
window_t delayed_windows[$];
bit saw_rx_zero[8];int next_probe[8],next_recovery[8];bit recovery=0;longint unsigned bytes_at_resume=0;
function automatic int unsigned get_be(input bytes_t b,input int at,input int n=2);
 int unsigned v=0;for(int i=0;i<n;i++)v=(v<<8)|b[at+i];return v;
endfunction
function automatic int csum(input bytes_t b);
 int unsigned s=0;for(int i=0;i<b.size();i+=2)s+=(int'(b[i])<<8)|((i+1<b.size())?int'(b[i+1]):0);
 while(s>>16)s=(s&65535)+(s>>16);return (~s)&65535;
endfunction
task automatic put_be(ref bytes_t b,input int at,input int unsigned v,input int n=2);
 for(int i=0;i<n;i++)b[at+i]=v>>(8*(n-1-i));
endtask
task automatic send_packet(input int sid,input int unsigned seq,input int unsigned ack,input int flags,input int len=0,input bit opts=0,input int ws=3,input bit corrupt=0,input int data_offset=0,input int bad_option=-1);
 bytes_t b,p; s_axis_tcp_data_word a;int h=opts?(ws<0?44:48):40;
 for(int i=0;i<h+len;i++)b.push_back(0);
 b[0]='h45;b[8]=64;b[9]=6;put_be(b,2,b.size());put_be(b,12,'h0a000002,4);put_be(b,16,'h0a000001,4);
 put_be(b,20,40000+sid);put_be(b,22,my_port[sid]);put_be(b,24,seq,4);put_be(b,28,ack,4);b[32]=(h-20)/4<<4;if(data_offset!=0)b[32]=data_offset<<4;b[33]=flags;put_be(b,34,peer_window);
 if(opts)begin b[40]=2;b[41]=4;put_be(b,42,8960);if(ws>=0)begin b[44]=3;b[45]=3;b[46]=ws;end end
 if(bad_option>=0)begin b[40]=30;b[41]=bad_option;end
 for(int i=0;i<len;i++)b[h+i]=((seq-peer_isn[sid]-1+i)*17+5+sid);
 for(int i=12;i<20;i++)p.push_back(b[i]);p.push_back(0);p.push_back(6);p.push_back((b.size()-20)>>8);p.push_back(b.size()-20);
 for(int i=20;i<b.size();i++)p.push_back(b[i]);put_be(b,36,csum(p));p.delete();for(int i=0;i<20;i++)p.push_back(b[i]);put_be(b,10,csum(p));if(corrupt)b[36]^=1;
 if(packet_log)begin $fwrite(packet_log,"I %0d ",!corrupt);foreach(b[i])$fwrite(packet_log,"%02x",b[i]);$fwrite(packet_log,"\n");end
 for(int o=0;o<b.size();o+=K)begin a='0;for(int j=0;j<K&&o+j<b.size();j++)begin a.TDATA[j*8+:8]=b[o+j];a.TKEEP[j]=1;end a.TSTRB=a.TKEEP;a.TLAST=o+K>=b.size();s_axis_tcp_data_q.push_back(a);end
endtask
task automatic consume_packet;
 bytes_t p;int sid,h,len;int unsigned seq;ack_t delayed;window_t update;
 if(packet_out.size()<40)$fatal(1,"short packet");
 if(get_be(packet_out,2)!=packet_out.size())$fatal(1,"IPv4 length");
 // TOE leaves the IPv4 checksum for the downstream MAC encoder.
 if(get_be(packet_out,10)!=0)$fatal(1,"IPv4 checksum placeholder");
 for(int i=12;i<20;i++)p.push_back(packet_out[i]);p.push_back(0);p.push_back(6);p.push_back((packet_out.size()-20)>>8);p.push_back(packet_out.size()-20);
 for(int i=20;i<packet_out.size();i++)p.push_back(packet_out[i]);if(csum(p)!==0)$fatal(1,"TCP checksum");
 sid=get_be(packet_out,22)-40000;if(sid<0||sid>=n_sessions)$fatal(1,"peer port");
 packet_count[sid]++;last_flags[sid]=packet_out[33];last_ack[sid]=get_be(packet_out,28,4);last_seq[sid]=get_be(packet_out,24,4);
 seq=get_be(packet_out,24,4);h=20+(packet_out[32]>>4)*4;len=packet_out.size()-h;
 update.due=cycle+2500;update.sid=sid;update.ack=get_be(packet_out,28,4);update.window=get_be(packet_out,34)<<3;delayed_windows.push_back(update);
 if(packet_out[33]&2)begin
  if(packet_out.size()!=(expect_ws?48:44)||packet_out[40]!=2||get_be(packet_out,42)!=8960||(expect_ws && (packet_out[44]!=3||packet_out[46]!=3)))$fatal(1,"SYN MSS/WS");
  server_seq[sid]=seq+1;my_port[sid]=get_be(packet_out,20);
  if(active_open && !(packet_out[33]&16))send_packet(sid,peer_isn[sid],seq+1,18,0,1);
  else begin connected[sid]=1;if(auto_handshake)send_packet(sid,peer_seq[sid],seq+1,16);end
 end
 if(len>0)begin
  for(int i=0;i<len;i++)if(packet_out[h+i]!==byte'((seq-server_seq[sid]+i)*31+9+sid))$fatal(1,"TX payload sid=%0d seq=%0d byte=%0d",sid,seq,i);
  if(seq==server_seq[sid]+tx_count[sid])begin tx_count[sid]+=len;tx_bytes+=len;end
  else if(seq>server_seq[sid]+tx_count[sid])$fatal(1,"TX gap");
  if(mode=="functional" && !retransmit_armed && tx_count[sid]>80000)begin
   retransmit_armed=1;hold_acks=1;retransmit_seq=seq;
   for(int d=0;d<4;d++)send_packet(sid,peer_seq[sid],seq,16);
  end else if(hold_acks && seq==retransmit_seq)begin hold_acks=0;saw_retransmit=1;end
  if(auto_ack && !hold_acks)begin delayed.due=cycle+2500;delayed.sid=sid;delayed.ack=server_seq[sid]+tx_count[sid];delayed_acks.push_back(delayed);end
 end
 if(packet_log)begin $fwrite(packet_log,"O 1 ");foreach(packet_out[i])$fwrite(packet_log,"%02x",packet_out[i]);$fwrite(packet_log,"\n");end
 packet_out.delete();
endtask
task automatic service_models;
 logic [63:0] key;logic [71:0] cmd;int unsigned addr,n;beat_t b;
 s_axis_session_lup_rsp_word lr; s_axis_session_upd_rsp_word ur;
 s_axis_rx_data_req_word rr;s_axis_rxread_data_word rb;
 s_axis_txwrite_sts_word st;s_axis_txread_data_word rd;
 if(m_axis_open_conn_rsp_TVALID && m_axis_open_conn_rsp_TREADY)begin
  if(m_axis_open_conn_rsp_TDATA[23:16]!=1)$fatal(1,"active open failed");connected[0]=1;
 end
 if(m_axis_session_lup_req_TVALID && m_axis_session_lup_req_TREADY)begin
  key=m_axis_session_lup_req_TDATA[63:0];lr='0;lr.TDATA[63:0]=key;
  lr.TDATA[80]=sessions.exists(key);lr.TDATA[79:64]=sessions.exists(key)?sessions[key]:0;
  lr.TDATA[112:81]=m_axis_session_lup_req_TDATA[95:64];s_axis_session_lup_rsp_q.push_back(lr);
 end
 if(m_axis_session_upd_req_TVALID && m_axis_session_upd_req_TREADY)begin
  key=m_axis_session_upd_req_TDATA[95:32];ur='0;ur.TDATA[31:0]=m_axis_session_upd_req_TDATA[31:0];ur.TDATA[95:32]=key;ur.TDATA[111:96]=m_axis_session_upd_req_TDATA[111:96];ur.TDATA[112]=1;ur.TDATA[144:113]=m_axis_session_upd_req_TDATA[143:112];
  if(m_axis_session_upd_req_TDATA[31:0]==0)begin
   sessions[key]=m_axis_session_upd_req_TDATA[111:96];
   session_to_peer[m_axis_session_upd_req_TDATA[111:96]]={key[55:48],key[63:56]}-40000;
   peer_to_session[session_to_peer[m_axis_session_upd_req_TDATA[111:96]]]=m_axis_session_upd_req_TDATA[111:96];
  end else sessions.delete(key);
  s_axis_session_upd_rsp_q.push_back(ur);
 end
 if(m_axis_rxwrite_data_TVALID && m_axis_rxwrite_data_TREADY)begin
  b.data=m_axis_rxwrite_data_TDATA;b.keep=m_axis_rxwrite_data_TKEEP;b.last=m_axis_rxwrite_data_TLAST;rx_fifo.push_back(b);if(rx_fifo.size()>8192)$fatal(1,"FIFO overflow");if(rx_fifo.size()+s_axis_rxread_data_q.size()>peak_fifo)peak_fifo=rx_fifo.size()+s_axis_rxread_data_q.size();
 end
 if(rx_fifo.size() && s_axis_rxread_data_q.size()<1)begin
  b=rx_fifo.pop_front();rb='0;rb.TDATA=b.data;rb.TKEEP=b.keep;rb.TSTRB=b.keep;rb.TLAST=b.last;s_axis_rxread_data_q.push_back(rb);
 end
 if(m_axis_notification_TVALID && m_axis_notification_TREADY && m_axis_notification_TDATA[31:16]!=0)begin
  rr='0;rr.TDATA=m_axis_notification_TDATA[31:0];s_axis_rx_data_req_q.push_back(rr);expected_read_lengths.push_back(m_axis_notification_TDATA[31:16]);
 end
 if(m_axis_rx_data_rsp_metadata_TVALID && m_axis_rx_data_rsp_metadata_TREADY)begin
  if(!expected_read_lengths.size())$fatal(1,"unexpected RX metadata");
  app_sessions.push_back(m_axis_rx_data_rsp_metadata_TDATA);app_lengths.push_back(expected_read_lengths.pop_front());
 end
 // AXI metadata and data are independent; pair them after bounded buffering.
 if(m_axis_rx_data_rsp_TVALID && m_axis_rx_data_rsp_TREADY)begin
  b.data=m_axis_rx_data_rsp_TDATA;b.keep=m_axis_rx_data_rsp_TKEEP;b.last=m_axis_rx_data_rsp_TLAST;
  app_fifo.push_back(b);if(app_fifo.size()>8)$fatal(1,"application FIFO overflow");
 end
 if(app_fifo.size() && app_sessions.size())begin
  b=app_fifo.pop_front();
  if(!session_to_peer.exists(app_sessions[0]))$fatal(1,"unknown RX session");
  rx_model_session=session_to_peer[app_sessions[0]];
  for(int j=0;j<K;j++)if(b.keep[j])begin
   if(b.data[j*8+:8]!==byte'(rx_count[rx_model_session]*17+5+rx_model_session))$fatal(1,"RX payload sid=%0d offset=%0d",rx_model_session,rx_count[rx_model_session]);
   rx_count[rx_model_session]++;rx_bytes++;app_lengths[0]--;if(app_lengths[0]<0)$fatal(1,"RX request length exceeded");
  end
  if(b.last)begin if(app_lengths[0]!=0)$fatal(1,"early RX TLAST");void'(app_sessions.pop_front());void'(app_lengths.pop_front());end
  else if(app_lengths[0]==0)$fatal(1,"missing RX TLAST");
 end
 if(m_axis_tx_data_rsp_TVALID && m_axis_tx_data_rsp_TREADY)begin
  if($isunknown(m_axis_tx_data_rsp_TDATA))$fatal(1,"unknown TX response");
  if(m_axis_tx_data_rsp_TDATA[63:62]==0)accept_tx(session_to_peer[m_axis_tx_data_rsp_TDATA[15:0]],m_axis_tx_data_rsp_TDATA[31:16]);
  else if(m_axis_tx_data_rsp_TDATA[63:62]==2)begin tx_pending[session_to_peer[m_axis_tx_data_rsp_TDATA[15:0]]]=0;retry_len[session_to_peer[m_axis_tx_data_rsp_TDATA[15:0]]]=m_axis_tx_data_rsp_TDATA[31:16];retry_at[session_to_peer[m_axis_tx_data_rsp_TDATA[15:0]]]=cycle+100;tx_retries++;end
  else $fatal(1,"TX not connected");
 end
 if(m_axis_txwrite_cmd_TVALID && m_axis_txwrite_cmd_TREADY)write_cmds.push_back(m_axis_txwrite_cmd_TDATA);
 if(m_axis_txwrite_data_TVALID && m_axis_txwrite_data_TREADY)begin
  if(!write_cmds.size())$fatal(1,"write without cmd");cmd=write_cmds[0];addr=cmd[63:32];
  for(int j=0;j<K;j++)if(m_axis_txwrite_data_TKEEP[j])begin mem[addr+write_offset]=m_axis_txwrite_data_TDATA[j*8+:8];write_offset++;end
  if(m_axis_txwrite_data_TLAST)begin if(write_offset!=cmd[22:0])$fatal(1,"write length");void'(write_cmds.pop_front());write_offset=0;st='0;st.TDATA='h80;s_axis_txwrite_sts_q.push_back(st);end
 end
 if(m_axis_txread_cmd_TVALID && m_axis_txread_cmd_TREADY)read_cmds.push_back(m_axis_txread_cmd_TDATA);
 if(read_cmds.size() && s_axis_txread_data_q.size()<1)begin
  cmd=read_cmds[0];addr=cmd[63:32];rd='0;
  for(int j=0;j<K&&read_offset<cmd[22:0];j++)begin if(!mem.exists(addr+read_offset))$fatal(1,"read unwritten memory");rd.TDATA[j*8+:8]=mem[addr+read_offset];rd.TKEEP[j]=1;read_offset++;end
  rd.TSTRB=rd.TKEEP;rd.TLAST=read_offset==cmd[22:0];s_axis_txread_data_q.push_back(rd);
  if(rd.TLAST)begin void'(read_cmds.pop_front());read_offset=0;end
 end
 if(m_axis_tcp_data_TVALID && m_axis_tcp_data_TREADY)begin
  for(int j=0;j<K;j++)if(m_axis_tcp_data_TKEEP[j])begin if($isunknown(m_axis_tcp_data_TDATA[j*8+:8]))$fatal(1,"unknown valid TX byte");packet_out.push_back(m_axis_tcp_data_TDATA[j*8+:8]);end
  if(m_axis_tcp_data_TLAST)consume_packet();
 end
endtask
task automatic request_tx(input int sid,input int length);
 s_axis_tx_data_req_metadata_word meta;
 meta='0;meta.TDATA={16'(length),16'(peer_to_session[sid])};s_axis_tx_data_req_metadata_q.push_back(meta);tx_pending[sid]=1;
endtask
task automatic accept_tx(input int sid,input int length);
 s_axis_tx_data_req_word a;
 for(int o=0;o<length;o+=K)begin a='0;for(int j=0;j<K&&o+j<length;j++)begin a.TDATA[j*8+:8]=(tx_requested[sid]+o+j)*31+9+sid;a.TKEEP[j]=1;end a.TSTRB=a.TKEEP;a.TLAST=o+K>=length;s_axis_tx_data_req_q.push_back(a);end
 tx_requested[sid]+=length;tx_pending[sid]=0;
endtask
