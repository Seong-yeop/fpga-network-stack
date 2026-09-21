#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
width=${1:-128}
# Control/data channels: force the first valid transaction to wait 64 clocks.
for target in {0..12}; do
 TOE_TARGET=$target TOE_STALL=64 ./run_cases.sh "$width" xsim backpressure
done
# Multi-beat data: exercise middle and last beats, including long stalls.
for target in 0 3 4 10; do
 for duration in 1 2 7 1024 4096; do
  for position in 1 2; do
   TOE_TARGET=$target TOE_STALL=$duration TOE_POSITION=$position ./run_cases.sh "$width" xsim backpressure
  done
 done
done
for profile in 1 2 3 7; do
 TOE_PROFILE=$profile ./run_cases.sh "$width" xsim backpressure
done
for profile in 4 5 6; do
 for seed in 1 7 42 20260920; do
  TOE_PROFILE=$profile TOE_SEED=$seed ./run_cases.sh "$width" xsim backpressure
 done
done
