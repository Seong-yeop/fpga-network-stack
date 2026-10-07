# Usage: TOE_WIDTH_BYTES=64|128 TOE_STAGE=csim|synth vitis_hls -f run_hls.tcl
set here [file dirname [file normalize [info script]]]
set root [file dirname $here]
set width $::env(TOE_WIDTH_BYTES)
set stage $::env(TOE_STAGE)
if {$width ni {64 128} || $stage ni {csim synth}} {error "invalid width/stage"}
set flags "-std=c++14 -DFNS_DATA_WIDTH=$width -DFNS_ROCE_STACK_MAX_QPS=500 -DTCP_NODELAY=1 -DTCP_MSS=8960 -DTCP_STACK_MAX_SESSIONS=1000 -DRX_DDR_BYPASS=1 -DFAST_RETRANSMIT=1 -DWINDOW_SCALE=1"
cd $here
set case_name ""
if {[info exists ::env(TOE_CASE)]} {set case_name $::env(TOE_CASE)}
if {$case_name ne ""} {
 if {![regexp {^[a-z][a-z0-9_]*$} $case_name]} {error "invalid TOE_CASE"}
 set case_file $here/cases/tb_${case_name}.cpp
 if {![file exists $case_file]} {error "unknown C-sim case $case_name"}
 if {$stage ne "csim"} {error "case selection applies to C-sim only"}
 set project $here/work_${width}_extended
 file mkdir $project
 set selected [open $project/selected_tb.cpp w]
 puts $selected "#include \"$case_file\""
 close $selected
} else {set project $here/work_$width}
open_project $project
set_top toe
foreach src {../axi_utils.cpp ack_delay/ack_delay.cpp close_timer/close_timer.cpp event_engine/event_engine.cpp port_table/port_table.cpp probe_timer/probe_timer.cpp retransmit_timer/retransmit_timer.cpp rx_app_if/rx_app_if.cpp rx_app_stream_if/rx_app_stream_if.cpp rx_engine/rx_engine.cpp rx_sar_table/rx_sar_table.cpp sack_scoreboard/sack_scoreboard.cpp session_lookup_controller/session_lookup_controller.cpp state_table/state_table.cpp tx_app_if/tx_app_if.cpp tx_app_stream_if/tx_app_stream_if.cpp tx_engine/tx_engine.cpp tx_sar_table/tx_sar_table.cpp tx_app_interface/tx_app_interface.cpp toe.cpp} {
    add_files [file normalize $root/$src] -cflags $flags
}
if {$case_name eq ""} {add_files -tb $here/tb_csim.cpp -cflags $flags} else {add_files -tb $project/selected_tb.cpp -cflags $flags}
open_solution -flow_target vivado solution
set_part {xcu55c-fsvh2892-2L-e}
create_clock -period 4 -name default
if {$stage eq "csim"} {csim_design} else {
    if {![file exists $here/work_$width/solution/csim_pass]} {error "Run passing C-sim first"}
    csynth_design
}
exit
