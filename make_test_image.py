#!/usr/bin/env python3
import random
import sys

IMAGE_SIZE = 2 * 1024 * 1024  # 2 MB
OUTPUT     = "usb.img"

random.seed(42)
data = bytearray(random.getrandbits(8) for _ in range(IMAGE_SIZE))

# 12 embedded fragments at known offsets
fragments = [
    (0x001000, b"login: admin"),
    (0x002200, b"password=hunter2"),
    (0x005000, b"secret: topsecret123"),
    (0x008400, b"token: eyJhbGciOiJIUzI1NiJ9"),
    (0x00A000, b"apikey=sk-abc123456789"),
    (0x010000, b"http://malicious.example.com/payload"),
    (0x015000, b"ssh-rsa AAAAB3NzaC1yc2EAAAADAQABtest"),
    (0x020000, b"-----BEGIN RSA PRIVATE KEY-----"),
    (0x030000, b"user@gmail.com"),
    (0x040000, b"CVV: 123  credit_card: 4111111111111111"),
    (0x050000, b"bitcoin:1A2B3C4D5EF  wallet.dat"),
    (0x060000, b"ransom: pay now or your files stay encrypted"),
]

for offset, payload in fragments:
    data[offset:offset + len(payload)] = payload

with open(OUTPUT, "wb") as f:
    f.write(data)

print(f"[*] Generated {OUTPUT}  ({IMAGE_SIZE // (1024 * 1024)} MB)  —  {len(fragments)} fragments embedded")
for offset, payload in fragments:
    print(f"    0x{offset:06X}  {payload[:40].decode('ascii', errors='replace')}")
