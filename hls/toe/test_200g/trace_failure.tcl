# Capture only the TB-facing interfaces, not the entire synthesized hierarchy.
foreach pattern {/tb_toe/ap_clk /tb_toe/ap_rst* /tb_toe/*_T* /tb_toe/*_empty_n /tb_toe/*_full_n /tb_toe/*_read /tb_toe/*_write /tb_toe/*_din /tb_toe/*_dout /tb_toe/cycle /tb_toe/case_phase} {
    set signals [get_objects $pattern]
    if {[llength $signals]} {log_wave $signals}
}
run all
quit
