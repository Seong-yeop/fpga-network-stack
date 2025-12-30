#!/usr/bin/env python3
"""
TCP Test Vector Generator for TOE HLS Simulation

Generates test vector files (.dat) and golden output files (.gold)

Usage:
    python generate_test_vector.py -o output.dat
    python generate_test_vector.py --random --num-packets 10 -o test.dat
    python generate_test_vector.py --analyze io_fin_5.dat
"""

import argparse
import struct
import random
from dataclasses import dataclass
from typing import List
import socket


@dataclass
class TCPPacket:
    """TCP Packet definition"""
    src_ip: str = "10.10.10.10"
    dst_ip: str = "1.1.1.1"
    src_port: int = 137
    dst_port: int = 87
    seq_num: int = 0
    ack_num: int = 0
    flags: int = 0x10
    window: int = 1024
    payload: bytes = b""

    FIN = 0x01
    SYN = 0x02
    RST = 0x04
    PSH = 0x08
    ACK = 0x10
    URG = 0x20


def ip_to_int(ip_str: str) -> int:
    return struct.unpack("!I", socket.inet_aton(ip_str))[0]


def compute_tcp_checksum(src_ip: int, dst_ip: int, tcp_header: bytes, payload: bytes) -> int:
    tcp_length = len(tcp_header) + len(payload)
    pseudo_header = struct.pack("!IIBBH", src_ip, dst_ip, 0, 6, tcp_length)
    data = pseudo_header + tcp_header + payload
    if len(data) % 2:
        data += b'\x00'
    checksum = 0
    for i in range(0, len(data), 2):
        word = (data[i] << 8) + data[i + 1]
        checksum += word
        checksum = (checksum & 0xFFFF) + (checksum >> 16)
    return ~checksum & 0xFFFF


def compute_ip_checksum(header: bytes) -> int:
    if len(header) % 2:
        header += b'\x00'
    checksum = 0
    for i in range(0, len(header), 2):
        word = (header[i] << 8) + header[i + 1]
        checksum += word
        checksum = (checksum & 0xFFFF) + (checksum >> 16)
    return ~checksum & 0xFFFF


def build_packet(pkt: TCPPacket) -> bytes:
    src_ip = ip_to_int(pkt.src_ip)
    dst_ip = ip_to_int(pkt.dst_ip)

    ip_total_length = 20 + 20 + len(pkt.payload)
    ip_header_no_checksum = struct.pack("!BBHHHBBH",
        0x45, 0x00, ip_total_length, 0x0000, 0x0000, 0xFF, 0x06, 0x0000
    ) + struct.pack("!II", src_ip, dst_ip)

    ip_checksum = compute_ip_checksum(ip_header_no_checksum)
    ip_header = struct.pack("!BBHHHBBH",
        0x45, 0x00, ip_total_length, 0x0000, 0x0000, 0xFF, 0x06, ip_checksum
    ) + struct.pack("!II", src_ip, dst_ip)

    data_offset = 5 << 4
    tcp_header_no_checksum = struct.pack("!HHIIHHHH",
        pkt.src_port, pkt.dst_port, pkt.seq_num, pkt.ack_num,
        (data_offset << 8) | pkt.flags, pkt.window, 0x0000, 0x0000
    )

    tcp_checksum = compute_tcp_checksum(src_ip, dst_ip, tcp_header_no_checksum, pkt.payload)
    tcp_header = struct.pack("!HHIIHHHH",
        pkt.src_port, pkt.dst_port, pkt.seq_num, pkt.ack_num,
        (data_offset << 8) | pkt.flags, pkt.window, tcp_checksum, 0x0000
    )

    return ip_header + tcp_header + pkt.payload


def bytes_to_words(data: bytes) -> List[tuple]:
    """Convert bytes to 64-bit words (little endian)"""
    words = []
    for i in range(0, len(data), 8):
        chunk = data[i:i+8]
        is_last = (i + 8 >= len(data))
        keep = (1 << len(chunk)) - 1
        chunk = chunk.ljust(8, b'\x00')

        word_val = 0
        for j, b in enumerate(chunk):
            word_val |= (b << (j * 8))

        words.append((word_val, 1 if is_last else 0, keep))
    return words


def format_word(data: int, last: int, keep: int) -> str:
    return f"{data:016X} {last} {keep:02X}"


class TCPSession:
    def __init__(self, src_ip="10.10.10.10", dst_ip="1.1.1.1",
                 src_port=137, dst_port=87, initial_seq=None):
        self.src_ip = src_ip
        self.dst_ip = dst_ip
        self.src_port = src_port
        self.dst_port = dst_port
        self.seq_num = initial_seq if initial_seq else random.randint(0x10000000, 0x7FFFFFFF)
        self.ack_num = 0
        self.payloads = []

    def make_syn(self) -> TCPPacket:
        return TCPPacket(
            src_ip=self.src_ip, dst_ip=self.dst_ip,
            src_port=self.src_port, dst_port=self.dst_port,
            seq_num=self.seq_num, ack_num=0,
            flags=TCPPacket.SYN, window=1024
        )

    def make_ack(self) -> TCPPacket:
        return TCPPacket(
            src_ip=self.src_ip, dst_ip=self.dst_ip,
            src_port=self.src_port, dst_port=self.dst_port,
            seq_num=self.seq_num, ack_num=self.ack_num,
            flags=TCPPacket.ACK, window=1024
        )

    def make_data(self, payload: bytes) -> TCPPacket:
        pkt = TCPPacket(
            src_ip=self.src_ip, dst_ip=self.dst_ip,
            src_port=self.src_port, dst_port=self.dst_port,
            seq_num=self.seq_num, ack_num=self.ack_num,
            flags=TCPPacket.ACK, window=1024, payload=payload
        )
        self.seq_num += len(payload)
        self.payloads.append(payload)
        return pkt

    def make_fin(self) -> TCPPacket:
        return TCPPacket(
            src_ip=self.src_ip, dst_ip=self.dst_ip,
            src_port=self.src_port, dst_port=self.dst_port,
            seq_num=self.seq_num, ack_num=self.ack_num,
            flags=TCPPacket.FIN | TCPPacket.ACK, window=1024
        )

    def receive_synack(self, peer_seq: int):
        self.ack_num = peer_seq + 1
        self.seq_num += 1


def generate_gold_file(payloads: List[bytes], gold_file: str):
    """Generate golden output file (TCP payloads only)"""
    lines = []
    for payload in payloads:
        words = bytes_to_words(payload)
        for word in words:
            lines.append(format_word(*word))

    with open(gold_file, 'w') as f:
        f.write('\n'.join(lines) + '\n')

    print(f"Generated {gold_file}")
    print(f"  - {len(payloads)} payloads")


def generate_test_vector(output_file: str, num_data_packets: int = 10,
                         payload_size: int = 41, wait_cycles: int = 100,
                         generate_gold: bool = True, random_mode: bool = False,
                         min_payload: int = 1, max_payload: int = 1460):
    """Generate complete TCP session: SYN -> ACK -> DATA*N -> FIN"""
    lines = []
    session = TCPSession(initial_seq=0x9F767C48)

    # SYN
    lines.append(f"W 200")
    for word in bytes_to_words(build_packet(session.make_syn())):
        lines.append(format_word(*word))

    # ACK
    lines.append(f"W {wait_cycles}")
    session.receive_synack(0)
    for word in bytes_to_words(build_packet(session.make_ack())):
        lines.append(format_word(*word))

    # DATA packets
    total_payload_bytes = 0
    for i in range(num_data_packets):
        lines.append(f"W {wait_cycles}")

        # Determine payload size
        if random_mode:
            pkt_payload_size = random.randint(min_payload, max_payload)
        else:
            pkt_payload_size = payload_size

        # Generate random payload
        payload = bytes([random.randint(0, 255) for _ in range(pkt_payload_size)])
        total_payload_bytes += len(payload)

        for word in bytes_to_words(build_packet(session.make_data(payload))):
            lines.append(format_word(*word))

    # FIN
    lines.append(f"W {wait_cycles}")
    for word in bytes_to_words(build_packet(session.make_fin())):
        lines.append(format_word(*word))

    # Write input file
    with open(output_file, 'w') as f:
        f.write('\n'.join(lines) + '\n')

    print(f"Generated {output_file}")
    if random_mode:
        print(f"  SYN -> ACK -> {num_data_packets}x DATA(random {min_payload}-{max_payload}B) -> FIN")
        print(f"  Total payload: {total_payload_bytes} bytes")
        print(f"  Payload sizes: {[len(p) for p in session.payloads]}")
    else:
        print(f"  SYN -> ACK -> {num_data_packets}x DATA({payload_size}B) -> FIN")

    # Generate gold file
    if generate_gold:
        gold_file = output_file.replace('.dat', '.gold')
        if gold_file == output_file:
            gold_file = output_file + '.gold'
        generate_gold_file(session.payloads, gold_file)


def analyze_file(input_file: str):
    """Analyze existing test vector file"""
    print(f"\n=== Analyzing {input_file} ===\n")

    with open(input_file, 'r') as f:
        lines = f.readlines()

    packet_num = 0
    current_packet = []

    for line in lines:
        line = line.strip()
        if not line:
            continue

        if line.startswith('W '):
            if current_packet:
                analyze_packet(packet_num, current_packet)
                packet_num += 1
                current_packet = []
            print(f"--- Wait {line.split()[1]} cycles ---")
        else:
            parts = line.split()
            if len(parts) >= 3:
                data = int(parts[0], 16)
                last = int(parts[1])
                keep = int(parts[2], 16)
                current_packet.append((data, last, keep))

                if last == 1:
                    analyze_packet(packet_num, current_packet)
                    packet_num += 1
                    current_packet = []


def analyze_packet(packet_num: int, words: List[tuple]):
    """Analyze a single packet"""
    print(f"\n[Packet {packet_num}] {len(words)} words")

    packet_bytes = bytearray()
    for data, last, keep in words:
        for i in range(8):
            if keep & (1 << i):
                packet_bytes.append((data >> (i * 8)) & 0xFF)

    if len(packet_bytes) < 40:
        print(f"  Too short: {len(packet_bytes)} bytes")
        return

    ip_total_length = (packet_bytes[2] << 8) | packet_bytes[3]
    src_ip = f"{packet_bytes[12]}.{packet_bytes[13]}.{packet_bytes[14]}.{packet_bytes[15]}"
    dst_ip = f"{packet_bytes[16]}.{packet_bytes[17]}.{packet_bytes[18]}.{packet_bytes[19]}"

    src_port = (packet_bytes[20] << 8) | packet_bytes[21]
    dst_port = (packet_bytes[22] << 8) | packet_bytes[23]
    seq_num = (packet_bytes[24] << 24) | (packet_bytes[25] << 16) | \
              (packet_bytes[26] << 8) | packet_bytes[27]
    ack_num = (packet_bytes[28] << 24) | (packet_bytes[29] << 16) | \
              (packet_bytes[30] << 8) | packet_bytes[31]
    flags = packet_bytes[33]

    flag_str = []
    if flags & 0x01: flag_str.append("FIN")
    if flags & 0x02: flag_str.append("SYN")
    if flags & 0x04: flag_str.append("RST")
    if flags & 0x08: flag_str.append("PSH")
    if flags & 0x10: flag_str.append("ACK")

    payload_len = ip_total_length - 40

    print(f"  IP: {src_ip} -> {dst_ip}, Total={ip_total_length}B")
    print(f"  TCP: {src_port} -> {dst_port}")
    print(f"  Seq=0x{seq_num:08X}, Ack=0x{ack_num:08X}")
    print(f"  Flags: {'+'.join(flag_str) if flag_str else 'None'}")
    if payload_len > 0:
        print(f"  Payload: {payload_len} bytes")
        if len(packet_bytes) > 40:
            print(f"  Data: {packet_bytes[40:56].hex()} ...")


def extract_gold_from_dat(input_file: str, gold_file: str):
    """Extract gold file from existing .dat file"""
    print(f"\n=== Extracting gold from {input_file} ===\n")

    with open(input_file, 'r') as f:
        lines = f.readlines()

    gold_lines = []
    current_packet = []

    for line in lines:
        line = line.strip()
        if not line or line.startswith('W '):
            if current_packet:
                packet_bytes = bytearray()
                for data, last, keep in current_packet:
                    for i in range(8):
                        if keep & (1 << i):
                            packet_bytes.append((data >> (i * 8)) & 0xFF)

                if len(packet_bytes) >= 40:
                    ip_total_length = (packet_bytes[2] << 8) | packet_bytes[3]
                    payload_len = ip_total_length - 40

                    if payload_len > 0:
                        payload = packet_bytes[40:40+payload_len]
                        for word in bytes_to_words(bytes(payload)):
                            gold_lines.append(format_word(*word))

                current_packet = []
            continue

        parts = line.split()
        if len(parts) >= 3:
            data = int(parts[0], 16)
            last = int(parts[1])
            keep = int(parts[2], 16)
            current_packet.append((data, last, keep))

    if current_packet:
        packet_bytes = bytearray()
        for data, last, keep in current_packet:
            for i in range(8):
                if keep & (1 << i):
                    packet_bytes.append((data >> (i * 8)) & 0xFF)

        if len(packet_bytes) >= 40:
            ip_total_length = (packet_bytes[2] << 8) | packet_bytes[3]
            payload_len = ip_total_length - 40

            if payload_len > 0:
                payload = packet_bytes[40:40+payload_len]
                for word in bytes_to_words(bytes(payload)):
                    gold_lines.append(format_word(*word))

    with open(gold_file, 'w') as f:
        f.write('\n'.join(gold_lines) + '\n')

    print(f"Generated {gold_file}")
    print(f"  - {len(gold_lines)} lines")


def main():
    parser = argparse.ArgumentParser(description="TCP Test Vector Generator")
    parser.add_argument("-o", "--output", default="test_vector.dat", help="Output file")
    parser.add_argument("--analyze", help="Analyze existing test vector file")
    parser.add_argument("--extract-gold", help="Extract gold from existing .dat file")
    parser.add_argument("--gold-output", help="Gold file output path")
    parser.add_argument("--num-packets", type=int, default=10, help="Number of data packets")
    parser.add_argument("--payload-size", type=int, default=41, help="Payload size in bytes (fixed mode)")
    parser.add_argument("--random", action="store_true", help="Random payload size mode")
    parser.add_argument("--min-payload", type=int, default=1, help="Min payload size for random mode")
    parser.add_argument("--max-payload", type=int, default=1460, help="Max payload size for random mode")
    parser.add_argument("--no-gold", action="store_true", help="Don't generate gold file")
    parser.add_argument("--seed", type=int, help="Random seed for reproducibility")

    args = parser.parse_args()

    if args.seed:
        random.seed(args.seed)

    if args.analyze:
        analyze_file(args.analyze)
    elif args.extract_gold:
        gold_out = args.gold_output or args.extract_gold.replace('.dat', '.gold')
        extract_gold_from_dat(args.extract_gold, gold_out)
    else:
        generate_test_vector(
            args.output,
            args.num_packets,
            args.payload_size,
            generate_gold=not args.no_gold,
            random_mode=args.random,
            min_payload=args.min_payload,
            max_payload=args.max_payload
        )


if __name__ == "__main__":
    main()
