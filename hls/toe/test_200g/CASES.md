# 개별 TOE 추가 TB

각 시나리오는 `cases/tb_<name>.cpp`와 `cases/tb_<name>.svh`에 분리했다.
C++ 모델은 `common/csim.hpp`, SV 패킷/메모리 모델은 `common/xsim.svh`,
SV 클록·초기화·bounded queue 구동은 `common/extended.svh`에서 공유한다.
선택한 SV 파일 하나를 실제 생성 RTL과 결합해 독립 XSim top을 만든다.
새 packetdrill parser나 별도 시뮬레이션 프레임워크는 추가하지 않았다.

## 실행

```sh
# 이 디렉터리에서 실행. 최초 1회 설치(오프라인 packet checker 전용).
python3 -m venv .venv
.venv/bin/pip install -r requirements-test.txt

# 개별 C-sim → 기존 top 합성 → 개별 XSim
./run_cases.sh 128 csim errors
./run.sh 128 csim
./run.sh 128 synth
./run_cases.sh 128 xsim errors

# 모든 독립 case (각 case는 새 simulation process로 실행)
./run_cases.sh 64 csim
./run_cases.sh 128 csim
./run_cases.sh 64 xsim
./run_cases.sh 128 xsim

# 특정 채널의 last beat를 4096 clocks 동안 정지
TOE_TARGET=10 TOE_STALL=4096 TOE_POSITION=2 ./run_cases.sh 128 xsim backpressure

# Seed 고정 무작위 ready 및 source idle
TOE_PROFILE=4 TOE_SEED=42 ./run_cases.sh 128 xsim errors

# Window Scale 제안: 미제안(-1), 0, 3, 7
TOE_WS=7 ./run_cases.sh 128 csim options
TOE_WS=7 ./run_cases.sh 128 xsim options

# 채널별/beat별/seed별 전체 backpressure 행렬
./run_backpressure.sh 64
./run_backpressure.sh 128
```

DUT 소스 변경 시 C-sim과 합성을 다시 수행한다. XSim runner는 해당 case의
C-sim PASS를 요구하고, RTL/TB fingerprint가 같을 때만 elaboration을 재사용한다.
`timers`와 `probe_timer`는 C-sim 컴포넌트 전용으로 `CSIM_ONLY`를 표시한다.

## 파일별 시나리오

| TB 이름 | 검증 내용 |
|---|---|
| `ack` | 미래/과거/부분 ACK 이후 유효 ACK와 후속 TX/RX |
| `reorder` | out-of-order, duplicate, overlap 드롭 및 in-order 재전송 복구 |
| `sequence_wrap` | peer ISN `0xfffffff0`, RX 32bit sequence wrap |
| `tx_sequence_wrap` | TX SAR 기존 요청 포트로 wrap 상태 구성, ACK/미확인 byte 계산과 응답 stall |
| `errors` | TCP checksum 오류, Data Offset < 5 또는 TCP 길이 초과, 이후 정상 수신 |
| `options` | MSS 광고와 WS 미제안/0/3/7, 연결 후 RX/TX |
| `malformed_options` | 길이 0/1/범위 초과 옵션의 parser 정지 방지; 다음 유효 옵션/연결 복구 |
| `connection` | 중복 SYN의 동일 ISN 유지, invalid RST 무시, valid RST 이후 수신 중단 |
| `active_open` | 애플리케이션 open → SYN/SYN-ACK → open status, RX/TX |
| `close` | local FIN 후 peer FIN+payload, payload 보존과 FIN sequence byte ACK |
| `closed_port` | 닫힌 포트 SYN의 RST 응답, 이후 listen/연결/수신 |
| `syn_exhaust` | MAX_SESSIONS개 연결이 찬 뒤 새 SYN에 RST+ACK(ack = seq + 1) 응답, 세션 수와 기존 세션의 수신 유지(빌드의 MAX_SESSIONS만큼 handshake를 하므로 v80_toe nvmet 변형(256)으로 실행) |
| `close_notify` | 앱 close 뒤의 피어 FIN 처리: payload 없는 FIN(FIN_WAIT_1/2)에 closed 통지 1회와 ACK, 재전송 FIN은 통지 없음, TIME_WAIT 만료는 통지 없이 해제; FIN+payload, FIN_WAIT에서 RST, TIME_WAIT에서 RST(통지 1-2회, 즉시 해제, 이후 stale close timer 활동 없음), 동시 close(CLOSING→TIME_WAIT, 통지 1회) |
| `fin_wait2_timeout` | 앱 close 뒤 피어가 ACK만 보내고 FIN을 보내지 않음: FIN_WAIT_2에서 데이터는 전달되고 RST가 없으며 같은 tuple의 SYN은 무시, 55-66 sweep 뒤 세션 해제와 closed 통지 1회, FIN 재전송 없음, 같은 tuple로 재접속 성공 |
| `fin_wait2_fin` | FIN_WAIT_2에서 30 sweep 뒤 피어 FIN: ACK, closed 통지 1회, TIME_WAIT 타이머가 FIN 시점부터 다시 시작(55-66 sweep 뒤 해제), 해제 통지 없음 |
| `fin_rto_exhaust` | 앱 close 뒤 피어 무응답: 같은 sequence의 FIN 7회(RTO 16/32/60/60/60/60 sweep), 340-360 sweep 뒤 해제와 closed 통지 1회, 이후 활동 없음 |
| `fin_wait2_rst_reuse` | MAX_SESSIONS개 연결이 찬 뒤 세션 0을 close(ACK → FIN_WAIT_2)하고 RST로 해제, 새 포트의 SYN이 같은 세션 ID를 재사용: FIN_WAIT_2에 걸린 close timer가 만료돼도(tA+66 sweep) 새 세션이 유지되고 통지가 없음 (빌드의 MAX_SESSIONS만큼 handshake) |
| `multisession` | 8개 tuple과 실제 session ID의 매칭, 혼합 RX/TX |
| `backpressure` | active open, RX/TX, fast retransmit의 전체 인터페이스 stall |
| `timers` | retransmit_timer 컴포넌트: C-sim 단축 상수(16/32/60 sweep)로 지수 backoff, 활성 타이머의 set 무시, 진전 ACK 재시작과 collapse, 6회 재전송 뒤 해제, 연속 사이클 set/stop forwarding. 실제 timeout 시간 인증 아님 |
| `probe_timer` | probe 만료 event, RX에 의한 fast resume, one-shot 동작 |
| `write_retransmit` | write가 정지된 동안 빠른 재전송을 유도해 write 완료 status 전 read 금지·완료 후 재개 검증 |
| `retransmit_window` | dup ACK 3개 → una의 1세그먼트 read 하나, 추가 dup ACK 무시, 누적 ACK 뒤 read 없음 |
| `retransmit_wrap` | 능동 open 세션 1에서 ring 끝에 걸친 세그먼트의 fast RT가 세션 자기 ring 안의 read 2개로 나뉨 |
| `dupack_window` | 같은 ack의 윈도 갱신은 dup ACK로 세지 않음(RFC 5681 §2), 윈도가 같은 dup ACK 3개만 fast RT |
| `recover_gate` | NewReno recover: partial ACK마다 1세그먼트, recover에서의 dup ACK는 재진입 금지, recover 너머는 허용, 종료 cwnd로 2세그먼트만 허용 |
| `retransmit_two_holes` | 구멍 두 개: 3번째 dup ACK가 첫 구멍, partial ACK가 둘째 구멍을 즉시 재전송 |
| `retransmit_tail` | dup ACK 없는 tail loss: 진전 ACK가 RTO를 재시작하고 16 sweep 뒤 RTO가 tail 1세그먼트를 재전송 (C-sim 전용) |
| `retransmit_lost` | fast RT 사본 재유실: dup ACK는 RTO를 재시작하지 않고 마지막 진전 ACK 기준 16 sweep 뒤 RTO가 반복 (C-sim 전용) |
| `rto_one_segment` | RTO 1세그먼트, LW가 앱 윈도에 반영, 2배 RTO, loss 상태의 진전 ACK마다 1세그먼트, backoff 유지와 collapse (C-sim 전용) |
| `sack_scoreboard` | sack_scoreboard 컴포넌트: 블록 병합·인접 결합·ACK 정리·가득 찼을 때 최저 블록 교체·ack 아래/역전/링 밖 블록 거부·clear·wrap·newData |
| `sack_negotiate` | SYN의 SACK-permitted 유무와 WS 유무별 SYN-ACK 옵션 배치·dataOffset, 능동 open SYN의 SACK-permitted |
| `sack_gro` | 세그먼트 3개를 덮는 SACK 블록 하나를 실은 ACK 하나로 una의 1세그먼트 fast RT(RFC 6675 IsLost), 반복 블록은 무시 |
| `sack_threshold` | SACK된 1 MSS와 정확히 2 MSS는 트리거하지 않고 2 MSS+1 B는 트리거(strict-greater) |
| `sack_blocks` | 불연속 블록 3개로 트리거, 이후 partial ACK마다 다음 hole 재전송(NewReno 순서) |
| `sack_dsack` | ack 아래 D-SACK 블록을 실은 데이터 세그먼트 3개는 dup으로 세지 않음, 둘째에 포함된 첫 블록은 한 번만 새 정보 |
| `sack_old_ack` | una보다 오래된 ack에 실린 SACK 블록은 무시, 현재 ack에 실리면 트리거 |
| `sack_with_data` | payload를 실은 ACK의 새 SACK 블록이 dup ACK로 세어져 fast RT(RFC 6675 §2) |
| `sack_not_negotiated` | 협상 없이 온 SACK 블록은 무시되고 dup ACK 3개 규칙만 적용 |
| `sack_rto_clear` | 1 MSS 블록만 있을 때 RTO, loss 상태에서 SACK 정보는 fast RT를 열지 않고 진전 ACK마다 1세그먼트 (C-sim 전용) |
| `sack_multiword` | SACK 블록을 실은 300 B 데이터 세그먼트(2워드 이상) 뒤의 순수 SACK ACK가 자기 블록으로 손실을 판정하고, 이런 세그먼트 3개가 이어져도 RX 데이터가 그대로 전달됨 (옵션 필드 FIFO 중복 쓰기 회귀, C-sim 전용) |
| `reconnect_after_loss` | RTO로 loss 상태가 된 연결을 부분 ACK+FIN과 앱 close로 닫은 뒤 다른 피어 포트의 새 연결에서 RX 700 B/TX 300 B를 30번 주고받음: 바이트 일치, 새 연결의 재전송 0, 추가 close 통지 0 (C-sim 전용) |

`tx_sequence_wrap`의 SV TB는 top 내부 값을 force하지 않는다. 기존 HLS 생성물의
`toe_tx_sar_table` 컴포넌트를 기존 FIFO 요청 인터페이스로 직접 구동한다.

## Backpressure와 oracle

- `PROFILE=0`: 기본 ready, `1/2/3`: ready 비율 1/2, 1/4, 3/4의 주기 패턴.
- `PROFILE=4/5/6`: ready 확률 1/4, 1/2, 3/4의 고정-seed PRBS 패턴.
- `PROFILE=7`: 전송 중 모든 출력 채널을 1024 clocks 정지한 뒤 재개한다.
- source idle은 새 beat 전에만 삽입한다. 이미 제시한 VALID는 handshake까지 유지한다.
- `TARGET`은 각 `xsim_<width>_backpressure/channels.txt`에 기록된 출력 채널 번호다.
- `POSITION=0/1/2`는 첫 transaction / multi-beat frame의 중간 / 마지막 beat다.
  첫 SYN 한 beat를 데이터의 middle/last stall로 잘못 세지 않도록 frame별 beat를 센다.
- 모든 출력 채널에 첫 transaction 64-cycle stall을 적용하고, 4개 데이터 출력에는
  1/2/7/1024/4096-cycle middle/last stall을 적용한다. 전체 행렬은 폭당 69개 조건이다.
- 메모리 outstanding 8, session response queue 4, application data buffer 8 beats.
  독립 metadata/data 채널은 두 FIFO의 순서로 매칭한다. metadata가 늦다는 이유만으로
  먼저 도착한 정상 data를 DUT 오류로 처리하지 않는다.
- 선택한 채널의 stall이 실제로 관찰되지 않으면 FAIL이다. 입력 및 출력 AXIS의
  VALID/data/keep/last 안정성 assertion과 byte/TLAST scoreboard를 함께 사용한다.
- TCP 패킷 trace는 Scapy 2.6.1로 재검사한다. checker 자체도 정상/오염 checksum으로
  self-test한다. TOE 출력 IPv4 checksum=0은 downstream MAC의 역할로 구분한다.

log 이름에 profile/seed/target/stall/position/WS가 들어간다. 실패 시 같은 조건으로
자동 재실행하여 외부 인터페이스 WDB와 trace log를 저장한다. 과거 실패 파형은
회귀 통과 후에도 남을 수 있으며, 당시 코드/로그 기준으로 해석한다. XSim의 exit status만으로
PASS를 판정하지 않고 SV PASS marker와 Scapy 결과를 모두 요구한다.

## 참고 저장소 10개

조회한 reference commit은 `cases/sources.json`에 고정했다. 아래 자료에서 검증
시나리오와 model 패턴을 선별했으며, 외부 stack 자체를 DUT의 정답으로 실행하지 않는다.

| 저장소 | 참고 지점 | 연결된 TB |
|---|---|---|
| [google/packetdrill](https://github.com/google/packetdrill) | packet/socket 이벤트 기반 검사 | ack, connection, active_open |
| [torvalds/linux](https://github.com/torvalds/linux/tree/master/tools/testing/selftests/net/packetdrill) | basic client/server, close/disorder 테스트 | active_open, close, closed_port |
| [lwip-tcpip/lwip](https://github.com/lwip-tcpip/lwip/blob/master/test/unit/tcp/test_tcp.c) | sequence wrap와 TCP 단위 테스트 | sequence_wrap, tx_sequence_wrap |
| [smoltcp-rs/smoltcp](https://github.com/smoltcp-rs/smoltcp/blob/main/src/socket/tcp.rs) | TCP 상태·sequence/window 처리 모델 | ack, reorder, connection |
| [google/gvisor](https://github.com/google/gvisor) | TCP e2e helper의 분할 전송·byte 검사 | multisession, backpressure의 byte scoreboard |
| [nsnam/ns-3-dev-git](https://github.com/nsnam/ns-3-dev-git/blob/master/src/internet/doc/tcp.rst) | zero-window, retransmit, timer/close 검사 | timers, close 및 기존 flow/full 회귀 |
| [secdev/scapy](https://github.com/secdev/scapy/blob/master/test/scapy/layers/inet.uts) | checksum, TCP options/data offset 검사 | errors, options, malformed_options; 실제 packet oracle |
| [alexforencich/cocotbext-axi](https://github.com/alexforencich/cocotbext-axi) | source/sink/monitor와 pause 패턴 | backpressure의 구동/수신 분리 |
| [alexforencich/verilog-axis](https://github.com/alexforencich/verilog-axis/blob/master/tb/axis_fifo/test_axis_fifo.py) | frame 길이, pause/stress/overflow 검사 | backpressure의 frame 경계·stall coverage |
| [fpgasystems/fpga-network-stack](https://github.com/fpgasystems/fpga-network-stack/blob/master/hls/toe/toe_tb.cpp) | TOE API와 기존 모델 | 공통 top/interface binding; 동일 계열이므로 독립 oracle 아님 |

RX bypass에서는 OOO 재조립을 요구하지 않고 드롭 후 재전송 복구를 검사한다.
SACK/Timestamp/ECN, 동적 peer-MSS 적용, IPv4 옵션/fragment/IP checksum 선별,
실제 RTO 시간과 physical place-and-route는 이번 추가 검증의 PASS 범위에 포함하지 않는다.
