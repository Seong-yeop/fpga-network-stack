# 개별 TOE TB 추가 검증

검증 환경: Vitis HLS/XSim 2024.2, U55C, 512/1024bit, 4ns/250MHz,
MSS 8960, RX bypass, TCP_NODELAY=1. 기존 200G 검증에 이어 수행한 추가 검증이다.

## 구성

- TB 16개를 각각 `cases/tb_<name>.cpp`로 분리했다.
- 타이머 컴포넌트 2개를 제외한 14개는 각각 대응하는 `.svh`가 있다.
- 클록·AXI 구동·packet/memory model만 공통 파일에서 공유한다.
- Scapy 2.6.1로 입출력 packet trace의 checksum·길이를 별도로 검사한다.
- 조사한 10개 저장소와 적용 항목은 [CASES.md](CASES.md), 참조 commit은
  [sources.json](cases/sources.json)에 기록했다. 외부 프로젝트 전체를 도입하지 않았다.

## 발견한 문제와 수정 범위

| 재현 | 원인 | 수정 |
|---|---|---|
| `errors`: early RX TLAST | Data Offset < 5 또는 TCP 길이보다 큰 header를 허용하여 잘못된 payload 길이 발행 | 메타데이터/payload 전달 전 header 길이 검사 |
| `malformed_options`: parser stalled | 길이 0인 옵션에서 진행하지 못함. 남은 옵션 길이의 word/byte 단위 혼재 | byte 단위 잔여량과 옵션 길이 검증. 잘못된 옵션은 WS 미협상으로 종료 |
| TX write middle stall + fast retransmit: read unwritten memory | TCP_NODELAY 최초 wire 전송이 TX buffer write 완료보다 빠름 | 완료 sequence의 wrap epoch 추적 및 RT read의 write 완료 대기 |
| RX metadata stall: RX missing metadata | TB가 독립 AXI 채널의 data가 metadata보다 먼저 수락될 수 없다고 가정 | TB에 8-beat bounded application queue를 추가해 metadata와 매칭 |

제품 코드의 이번 추가 수정은 `rx_engine.cpp`, `tx_engine.cpp`, `tx_sar_table.cpp`,
`toe_internals.hpp`에 한정했다. 이전 200G 폭 확장 변경과 구분한다.

재전송 보호는 메모리 모델을 느슨하게 바꾼 것이 아니다. TB의 미기록 주소 read 검사는
유지한다. 내부 완료 포인터만 32bit로 보관하고, 기존 ordered write status로 low-bit
wrap을 복원한다. SAR reply에 내부 `writesPending` 1bit를 추가했으며, RT가 재조회하는
사이에 query-free cycle을 두어 완료 갱신이 굶지 않게 한다. 외부 TOE 포트는 그대로다.
TX wrap 컴포넌트 TB는 실제 생성된 SAR 모듈의 요청 포트를 사용하며 내부 force는 없다.

## 최종 실행 결과

| 검증 | 결과 |
|---|---|
| 개별 C-sim 기능/컴포넌트 16개 × 2폭 | 32/32 PASS |
| 개별 XSim 14개 × 2폭 | 28/28 PASS; 타이머 2개는 별도 C-sim 전용 |
| Backpressure 69조건 × 2폭 | 138/138 PASS |
| errors/reorder/multisession + 무작위 stall, 각 2폭 | 6/6 PASS |
| WS 0/3/7 추가 조건 × 2폭 XSim | 6/6 PASS; WS 미제안은 기본 case 포함 |
| packet trace Scapy 교차 검사 | 모든 packet 기반 XSim case PASS |
| 1024bit 8세션 양방향, RTT 10us, 1ms | RX **241.985536Gbps**, TX **248.514560Gbps**, 양쪽 200G 목표 PASS |

각 폭의 backpressure 69조건은 출력 13개 채널의 첫 transaction stall,
4개 데이터 채널 × 5개 길이 × middle/last 40조건, 주기 패턴 3조건,
전체 1024-cycle 정지 1조건, PRBS 3확률 × 4seed 12조건이다.
네 seed는 1, 7, 42, 20260920이다. 선택한 stall이 실제로 발생하지 않으면 실패한다.

최종 HLS raw estimate는 두 폭 모두 **3.621ns / 276.13MHz**다.
metadata loader와 payload 처리 경로의 II=1을 유지한다. 기존 retransmit timer II=2와
기본 uncertainty 1.08ns에 대한 timing warning은 남아 있다. 이는 physical timing closure가 아니다.

### 결과 위치

- C-sim: `work_<64|128>_extended/<case>.log`
- XSim: `xsim_<64|128>_<case>/p<profile>_s<seed>_t<target>_n<stall>_b<position>_w<ws>.log`
- 각 로그의 `PASS <case>`와 `PASS Scapy`를 함께 확인한다. TX SAR 컴포넌트는 packet oracle 대상이 아니다.
- `COVER`/`QUEUE`에는 channel별 transfer/stall 및 종료 시 queue 상태가 기록된다.
- 실패 시 동일 이름의 `.wdb`/`.trace.log`, packet trace는 `.hex`로 저장한다.
- [최종 처리량 로그](xsim_128/duplex_8sessions.log)
- [512bit HLS 보고서](work_64/solution/syn/report/toe_csynth.rpt), [1024bit HLS 보고서](work_128/solution/syn/report/toe_csynth.rpt)

## 해석 범위

- C-sim은 TCP 기능과 컴포넌트 상태를 검사하며 실제 RTL backpressure 검증을 대신하지 않는다.
- AXI stall은 XSim에서 handshake·필드 안정성·byte/TLAST·명령/응답 순서로 검사한다.
- RX bypass의 OOO 정책은 드롭 후 재전송 복구다. 다른 software stack의 재조립 정책을 강제하지 않는다.
- 타이머 2개는 C-sim의 기존 단축 상수를 사용한다. 실제 RTO/persist 시간의 인증이 아니다.
- SACK/Timestamp/ECN, 동적 peer-MSS, IPv4 fragment/options, 미완료 DMA와 abort의 동시성은 PASS 범위 밖이다.
- HLS raw clock estimate, uncertainty를 포함한 목표, XSim 4ns 구동, 실제 post-route timing을 구분한다.
  uncertainty 경고를 숨기기 위한 설정 변경이나 physical place-and-route는 수행하지 않았다.

TCP_NODELAY에서 큰 애플리케이션 요청의 자동 MSS 분할을 인증한 결과는 아니다.
추가 전송 TB와 처리량 기준은 segment 단위 요청을 사용한다. 기존 MSS+1 길이 테스트는 byte 보존 검사로 구분한다.
