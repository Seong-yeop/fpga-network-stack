#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
width=${1:-128}; stage=${2:-csim}; selected=${3:-all}
case "$width" in 64|128) ;; *) exit 2;; esac
cases=(ack reorder sequence_wrap errors options connection active_open multisession backpressure timers close closed_port malformed_options tx_sequence_wrap probe_timer write_retransmit retransmit_window retransmit_wrap stitch retransmit_lost retransmit_two_holes)
if [[ "$selected" != all ]]; then
 [[ -f "cases/tb_$selected.cpp" ]] || { echo "Unknown case: $selected" >&2; exit 2; }
 cases=("$selected")
fi
mkdir -p "work_${width}_extended"
failed=0
for testcase in "${cases[@]}"; do
 case "$stage" in
 csim)
  log="work_${width}_extended/${testcase}.log"
  if TOE_CASE="$testcase" TOE_WIDTH_BYTES="$width" TOE_STAGE=csim \
     "${TOE_HLS_BIN:-/opt/Xilinx/Vitis_HLS/2024.2/bin/vitis_hls}" -f run_hls.tcl > "$log" 2>&1 \
     && grep -q "^PASS $testcase width=" "$log"; then
   echo "PASS csim $width $testcase"
  else
   echo "FAIL csim $width $testcase ($log)"; failed=1
  fi
  ;;
 xsim)
  if [[ ! -f "cases/tb_$testcase.svh" ]]; then echo "CSIM_ONLY $testcase"; continue; fi
  grep -q "^PASS $testcase width=" "work_${width}_extended/${testcase}.log" || { echo "Run passing C-sim first: $testcase"; exit 1; }
  TOE_CASE="$testcase" python3 generate_xsim.py "$width"
  simdir="xsim_${width}_${testcase}"
  (
   cd "$simdir" || exit 1
   vb=${TOE_VIVADO_BIN:-/opt/Xilinx/Vivado/2024.2/bin}
   if [[ ! -f compiled.sha256 ]] || ! cmp -s inputs.sha256 compiled.sha256; then
    "$vb/xvlog" -sv -i .. -f sources.f > compile.log 2>&1 || exit 1
    "$vb/xelab" tb_toe -s toe_case --debug typical > elaborate.log 2>&1 || exit 1
    cp inputs.sha256 compiled.sha256
   fi
   label="p${TOE_PROFILE:-0}_s${TOE_SEED:-1}_t${TOE_TARGET:--1}_n${TOE_STALL:-0}_b${TOE_POSITION:-0}_w${TOE_WS:--1}"
   args=(toe_case -testplusarg "SEED=${TOE_SEED:-1}" -testplusarg "PROFILE=${TOE_PROFILE:-0}"
     -testplusarg "TARGET=${TOE_TARGET:--1}" -testplusarg "STALL=${TOE_STALL:-0}" -testplusarg "POSITION=${TOE_POSITION:-0}"
     -testplusarg "WS=${TOE_WS:--1}")
   passed=1
   "$vb/xsim" "${args[@]}" -runall -log "${label}_xsim.log" > "$label.log" 2>&1 || passed=0
   grep -q "^PASS $testcase width=" "$label.log" || passed=0
   if [[ "$testcase" != tx_sequence_wrap ]]; then
    ../.venv/bin/python ../check_packets.py packets.hex >> "$label.log" 2>&1 || passed=0
    [[ ! -f packets.hex ]] || cp packets.hex "$label.hex"
   fi
   if (( ! passed )); then
    "$vb/xsim" "${args[@]}" -tclbatch ../trace_failure.tcl -wdb "$label.wdb" > "$label.trace.log" 2>&1 || true
    exit 1
   fi
  ) && echo "PASS xsim $width $testcase" || { echo "FAIL xsim $width $testcase ($simdir)"; failed=1; }
  ;;
 *) echo "Unsupported stage: $stage" >&2; exit 2;;
 esac
done
exit "$failed"
