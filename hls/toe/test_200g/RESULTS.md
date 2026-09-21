# TOE 200G 검증 결과

초기 200G 확장 검증 기록이다. 후속 TB와 수정의 최신 결과는
[EXTENDED_RESULTS.md](EXTENDED_RESULTS.md)를 참조한다. 빌드 산출물 링크는 재실행 시 갱신된다.

검증일: 2026-09-20. 기준 서브모듈: `337b167757e108aea0accda7e7fef57bed352bf2`.
도구: Vitis HLS / Vivado XSim 2024.2. 대상 part: `xcu55c-fsvh2892-2L-e`.

## 설정과 범위

- 1024bit, 250MHz / 4ns, MSS 8960, RX DDR bypass, TCP_NODELAY 및 Window Scale 활성화.
- 512bit 회귀 검증도 같은 4ns 목표로 수행했다.
- 실제 `toe` top C-sim을 먼저 통과한 뒤 HLS RTL을 생성하고 독립 SV TB로 XSim을 실행했다. HLS co-simulation은 사용하지 않았다.
- 전체 시스템 CMake/Makefile, MAC, NVMe, 메모리 컨트롤러, 타이머 및 윈도 정책은 변경하지 않았다.

## 기능 검증

| 검증 | 결과 |
|---|---|
| C-sim 512bit / 1024bit | 모두 PASS, 각각 96,140 top 호출 |
| checksum, keep 길이 0~beat bytes, 정렬 offset 전체 | PASS |
| 실제 TX packet pusher의 모든 offset에 대한 메모리 경계 분할 | PASS |
| FIFO entry→byte 윈도 환산, full/초과 occupancy, 최대량 0 | PASS |
| top RX/TX 길이 1/63/64/65/127/128/129/8959/8960/8961 | PASS |
| top TX 256KiB ring wrap 및 FIN | PASS |
| XSim 512bit / 1024bit 기능 | 모두 PASS, 각각 RX/TX 314,177byte 비교 |
| XSim SYN MSS/WS, 무작위 backpressure, zero-window/reopen | PASS |
| C-sim/XSim 수신 요청 길이와 TLAST 경계 일치 | PASS |
| XSim duplicate ACK에 의한 TX 재전송 | PASS; 재전송 관찰이 없으면 TB가 실패함 |
| RX 정지·재개 (`flow`) | PASS; peak occupancy 7510, zero-window 64회, 이후 reopen 8회 |
| RX 보호 경계·재전송 복구 (`full`) | PASS; peak occupancy 7860, 배출 후 FIFO empty 및 전송/수신 byte 수 일치 |

`full`은 **물리 FIFO full을 강제로 만드는 테스트가 아니다.** 기존 RX FSM은 잔여
375 entries 이하에서 수신을 제한한다. 윈도를 무시하는 스트레스 입력에도 이
보호를 유지했고, threshold 도달·overflow 방지·재전송 후 데이터 복구를 검증했다.
이 보호를 우회하도록 제품 코드를 수정하지 않았다. 375 entries의 byte 여유는
512bit에서 24,000byte, 1024bit에서 48,000byte로 달라진다.

C-sim 종료 시 free-session/free-port pool에 데이터가 남았다는 경고가 발생한다.
이는 상시 동작하는 TOE의 자원 pool이며, PASS는 별도 payload/command/timeout
검사에 기반한다. 경고 자체를 통과 기준으로 사용하지 않았다.

## XSim 처리량

8개 세션, 각 방향의 ACK 피드백 지연 2500 clocks = 10us. 트래픽 warmup 후
250,000 clocks = 1ms 동안 전달된 TCP payload만 계산한다. TCP/IP 헤더와
중복 재전송 byte는 제외한다. 애플리케이션과 모델 메모리는 native 1024bit로
동작한다. 아래는 behavioral 메모리 환경의 TOE IP 처리량이며 HBM/SSD 성능이 아니다.

| 모드 | RX Gbit/s | TX Gbit/s | 200G 목표 |
|---|---:|---:|---|
| RX 단독 | 248.889344 | — | PASS |
| TX 단독 | — | 248.872960 | PASS |
| 양방향 | 241.985536 | 248.514560 | 양쪽 PASS |
| 단일 세션 양방향 (별도 측정) | 49.975296 | 201.779200 | 참고값 |

단일 세션 RX는 현재 최대 광고 윈도 `8 × MSS = 71,680byte`와 ACK 주기에
제한된다. 이 윈도의 10us RTT 기준 단순 상한은 57.344Gbit/s이며, 8세션 목표와
구분해서 기록했다. 단일 세션 RX를 200G로 만들기 위해 윈도 정책을 변경하지 않았다.

## HLS 추정과 한계

| 항목 | 512bit | 1024bit |
|---|---:|---:|
| 설정 주기 | 4ns | 4ns |
| HLS 추정 주기 | 3.378ns | 3.539ns |
| HLS 추정 Fmax | 296.03MHz | 282.59MHz |
| LUT | 127280 | 163044 |
| FF | 140092 | 180295 |
| BRAM_18K | 272 | 272 |
| DSP / URAM | 0 / 0 | 0 / 0 |

**raw HLS 추정 주기는 4ns보다 짧지만, timing closure 완료를 의미하지 않는다.**
HLS 기본 uncertainty 1.08ns에 따른 유효 budget은 2.92ns이며 일부 경로에 경고가
남는다. uncertainty를 낮춰 경고를 숨기지 않았다. payload 처리 경로는 II=1이고,
기존 retransmit timer의 II=2가 top 요약에도 반영된다. 타이머 정책/순회 구조는
변경하지 않았다. 실제 FPGA 배치배선과 post-route timing은 수행하지 않았다.
자원 수치는 HLS 보고서 추정이며 TB의 외부 RX FIFO와 메모리 모델은 포함하지 않는다.

## 수정 근거

1. 수정 전 512bit 기본 top 테스트는 통과했고, 1024bit에서는 65byte checksum부터 실패했다. 64→32 부분합 단계를 추가했다.
2. RX IPv4 header 제거의 폭 분기 누락 때문에 1024bit SYN 처리가 정지했다. 해당 경로에만 1024bit를 추가했다.
3. SYN MSS/WS 생성, keep 길이와 정렬, FIFO byte 환산의 512bit 가정을 제거했다.
4. 경계 테스트에서 기존 512bit에도 발생한 잔여 word의 미초기화 keep을 0 초기화로 수정했다.
5. XSim에서 TX 윈도 응답의 X 전파가 확인되어 두 `scaled_recv_window` 변수를 0 초기화했다.
6. 처음 구현한 keep encoder는 stitching 경로가 21.101ns, 합산 방식은 8.862ns였다. 최종 병렬 OR encoder로 top 추정 3.539ns를 얻었다. 추가 FIFO 확대나 타이머 개편은 하지 않았다.

재실행 방법은 [README.md](README.md), 제품 코드와 TB의 변경점은 서브모듈에서
`git diff`로 확인할 수 있다. 생성 로그와 RTL은 `work_*`, `xsim_*`에 보존되며 git에서는 제외한다.

## 로컬 원본 결과

- [512bit C-sim](work_64/csim.log), [1024bit C-sim](work_128/csim.log)
- [512bit XSim 기능](xsim_64/functional_1sessions.log), [1024bit XSim 기능](xsim_128/functional_1sessions.log)
- [RX 8세션](xsim_128/rx.log), [TX 8세션](xsim_128/tx_final/run.log), [양방향 8세션](xsim_128/duplex/run.log)
- [양방향 단일 세션](xsim_128/duplex.log)
- [RX 정지·재개](xsim_128/flow.log), [RX 보호 경계·복구](xsim_128/full_final/run.log)
- [512bit HLS 보고서](work_64/solution/syn/report/toe_csynth.rpt), [1024bit HLS 보고서](work_128/solution/syn/report/toe_csynth.rpt)

원본 로그는 현재 workspace에 존재하는 빌드 산출물이다. 새 checkout에서는
README의 명령으로 재생성하며, 최종 스크립트는 mode/session 수를 붙인 로그명을 사용한다.
