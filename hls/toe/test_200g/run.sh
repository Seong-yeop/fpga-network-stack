#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
width=${1:-128}
stage=${2:-csim}
case "$width" in 64|128) ;; *) echo 'width must be 64 or 128 bytes' >&2; exit 2;; esac
export TOE_WIDTH_BYTES="$width" TOE_STAGE="$stage"
hls_bin=${TOE_HLS_BIN:-/opt/Xilinx/Vitis_HLS/2024.2/bin/vitis_hls}
vivado_bin=${TOE_VIVADO_BIN:-/opt/Xilinx/Vivado/2024.2/bin}
mkdir -p "work_$width"
case "$stage" in
 csim)
  rm -f "work_$width/solution/csim_pass"
  "$hls_bin" -f run_hls.tcl 2>&1 | tee "work_$width/csim.log"
  ;;
 synth)
  "$hls_bin" -f run_hls.tcl 2>&1 | tee "work_$width/synth.log"
  ;;
 xsim)
  python3 generate_xsim.py "$width"
  cd "xsim_$width"
  "$vivado_bin/xvlog" -sv -i .. -f sources.f > compile.log 2>&1
  "$vivado_bin/xelab" tb_toe -s toe_sim --debug typical > elaborate.log 2>&1
  mode=${3:-functional}
  sessions=${TOE_SESSIONS:-8}
  if [[ "$mode" == functional ]]; then sessions=1; fi
  label="${mode}_${sessions}sessions"
  "$vivado_bin/xsim" toe_sim -runall -testplusarg "MODE=$mode" -testplusarg "SESSIONS=$sessions" -log "${label}_xsim.log" 2>&1 | tee "${label}.log"
  grep -q '^PASS ' "${label}.log"
  ;;
 *) echo 'stage must be csim, synth or xsim' >&2; exit 2;;
esac
