open_project toe_prj

set_top toe

# Configuration flags
set cflags "-DFNS_DATA_WIDTH=64 -DFNS_ROCE_STACK_MAX_QPS=500 -DTCP_MSS=4096 -DTCP_NODELAY=1 -DTCP_STACK_MAX_SESSIONS=1000 -DRX_DDR_BYPASS=0 -DFAST_RETRANSMIT=1 -DWINDOW_SCALE=1"

add_files ../axi_utils.cpp -cflags $cflags
add_files ack_delay/ack_delay.cpp -cflags $cflags
add_files close_timer/close_timer.cpp -cflags $cflags
add_files event_engine/event_engine.cpp -cflags $cflags
add_files port_table/port_table.cpp -cflags $cflags
add_files probe_timer/probe_timer.cpp -cflags $cflags
add_files retransmit_timer/retransmit_timer.cpp -cflags $cflags
add_files rx_app_if/rx_app_if.cpp -cflags $cflags
add_files rx_app_stream_if/rx_app_stream_if.cpp -cflags $cflags
add_files rx_engine/rx_engine.cpp -cflags $cflags
add_files rx_sar_table/rx_sar_table.cpp -cflags $cflags
add_files session_lookup_controller/session_lookup_controller.cpp -cflags $cflags
#add_files session_lookup_controller/session_lookup_controller/stub_session_lookup.cpp -cflags $cflags
add_files state_table/state_table.cpp -cflags $cflags
add_files tx_app_if/tx_app_if.cpp -cflags $cflags
add_files tx_app_stream_if/tx_app_stream_if.cpp -cflags $cflags
add_files tx_engine/tx_engine.cpp -cflags $cflags
add_files tx_sar_table/tx_sar_table.cpp -cflags $cflags
add_files tx_app_interface/tx_app_interface.cpp -cflags $cflags
add_files dummy_memory.cpp -cflags $cflags
add_files toe.cpp -cflags $cflags
add_files -tb toe_tb.cpp -cflags $cflags

open_solution "solution1"
set_part {xc7vx690tffg1761-2}
create_clock -period 6.66 -name default

#csim_design -clean -argv {../../../../testVectors/io_finwp_5.dat ../../../../testVectors/rxOutput.dat ../../../../testVectors/txOutput.dat ../../../../testVectors/rx_io_finwp_5.gold}
#csim_design -clean -argv {0 ../../../../testVectors/io_fin_5.dat ../../../../testVectors/rxOutput.dat ../../../../testVectors/txOutput.dat ../../../../testVectors/rx_io_fin_5.gold}
csim_design -clean -argv {0 ../../../../testVectors/random.dat ../../../../testVectors/rxOutput.dat ../../../../testVectors/txOutput.dat ../../../../testVectors/random.gold}
#csim_design -clean -argv {0 ../../../../testVectors/mysyn.dat ../../../../testVectors/rxOutput.dat ../../../../testVectors/txOutput.dat ../../../../testVectors/rx_io_fin_5.gold}
#csim_design -clean -setup
#csynth_design
#cosim_design -tool modelsim -rtl verilog -trace_level all -argv {../../../../testVectors/in9.dat ../../../../testVectors/rxOutput.dat ../../../../testVectors/txOutput.dat}
#export_design -format ip_catalog -display_name "10G TCP Offload Engine" -description "TCP Offload Engine supporting 10Gbps line rate, up to 10K concurrent sessions & Out-Of-Order segments." -vendor "xilinx.labs" -version "2.06"
exit
