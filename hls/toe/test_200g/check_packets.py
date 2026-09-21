#!/usr/bin/env python3
"""Independent Scapy checksum/length oracle for the selected SV TB trace."""
import pathlib
import socket
import sys
from scapy.layers.inet import IP, TCP, in4_chksum
from scapy.utils import checksum

def valid_tcp(raw):
    ip = IP(raw)
    return in4_chksum(socket.IPPROTO_TCP, ip, raw[20:ip.len]) == 0

# Checker self-test: a one-byte corruption must be detected.
p = bytes(IP(src='10.0.0.2', dst='10.0.0.1')/TCP()/b'checker')
assert valid_tcp(p)
p = p[:-1] + bytes([p[-1] ^ 1])
assert not valid_tcp(p)
count = 0
for line in pathlib.Path(sys.argv[1]).read_text().splitlines():
    direction, expected, encoded = line.split()
    raw = bytes.fromhex(encoded)
    ip = IP(raw)
    assert ip.version == 4 and ip.ihl == 5 and ip.len == len(raw), line
    assert valid_tcp(raw) == bool(int(expected)), line
    if direction == 'I':
        assert checksum(raw[:20]) == 0, line
    else:
        assert ip.chksum == 0, 'TOE leaves IPv4 checksum to MAC encoder'
        assert 5 <= ip[TCP].dataofs <= 15 and ip[TCP].dataofs * 4 <= len(raw)-20, line
    count += 1
assert count, 'empty packet trace'
print(f'PASS Scapy packets={count}')
