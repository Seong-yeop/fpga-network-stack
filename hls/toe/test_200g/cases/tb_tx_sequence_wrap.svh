// A component TB around the unchanged synthesized toe_tx_sar_table module.
initial begin ap_clk=0;forever #2 ap_clk=~ap_clk;end
task automatic send_tx(input logic [52:0] q);
 @(negedge ap_clk);txEng2txSar_upd_req_dout=q;txEng2txSar_upd_req_empty_n=1;
 do @(posedge ap_clk);while(!txEng2txSar_upd_req_read);
 @(negedge ap_clk);txEng2txSar_upd_req_empty_n=0;
 repeat(8)@(negedge ap_clk);
endtask
initial begin
 logic [52:0] tq;logic [142:0] rq;
 component_defaults();ap_rst=1;repeat(10)@(negedge ap_clk);ap_rst=0;
 tq=0;tq[15:0]=7;tq[47:16]=32'hfffffff0;tq[48]=1;tq[49]=1;send_tx(tq);
 tq[47:16]=32'h80;tq[49]=0;send_tx(tq);
 rq=0;rq[15:0]=7;rq[47:16]=32'h40;rq[63:48]=65535;rq[81:64]=14600;rq[140:137]=3;rq[141]=1;rq[142]=1;
 rxEng2txSar_upd_req_dout=rq;rxEng2txSar_upd_req_empty_n=1;
 do @(posedge ap_clk);while(!rxEng2txSar_upd_req_read);
 @(negedge ap_clk);rxEng2txSar_upd_req_empty_n=0;repeat(8)@(negedge ap_clk);
 txSar2txEng_upd_rsp_full_n=0;tq=0;tq[15:0]=7;send_tx(tq);
 repeat(7)@(negedge ap_clk);txSar2txEng_upd_rsp_full_n=1;
 do @(posedge ap_clk);while(!txSar2txEng_upd_rsp_write);
 if(txSar2txEng_upd_rsp_din[31:0]!==32'h40 || txSar2txEng_upd_rsp_din[63:32]!==32'h80 || txSar2txEng_upd_rsp_din[117:100]!==18'd64)$fatal(1,"TX wrap accounting");
 if(txSar2txEng_upd_rsp_din[124]!==1'b1)$fatal(1,"uncommitted wrap marked readable");
 @(negedge ap_clk);txApp2txSar_push_dout={18'h80,16'd7};txApp2txSar_push_empty_n=1;
 do @(posedge ap_clk);while(!txApp2txSar_push_read);
 @(negedge ap_clk);txApp2txSar_push_empty_n=0;repeat(8)@(negedge ap_clk);
 tq=0;tq[15:0]=7;
 @(negedge ap_clk);txEng2txSar_upd_req_dout=tq;txEng2txSar_upd_req_empty_n=1;
 do @(posedge ap_clk);while(!txEng2txSar_upd_req_read);
 @(negedge ap_clk);txEng2txSar_upd_req_empty_n=0;
 do @(posedge ap_clk);while(!txSar2txEng_upd_rsp_write);
 if(txSar2txEng_upd_rsp_din[124]!==1'b0)$fatal(1,"commit did not clear wrap hazard");
 $display("PASS tx_sequence_wrap width=%0d",W);$finish;
end
initial begin #40000;$fatal(1,"TX SAR timeout");end
