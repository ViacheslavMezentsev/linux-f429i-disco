"""Check TC-08 with Windows Npcap on the interface connected to the board."""
import argparse
import ctypes
import os
import secrets
import socket
import struct
import time

HOST_IP = socket.inet_aton("198.18.42.1")
BOARD_IP = socket.inet_aton("198.18.42.2")
BOARD_MAC = bytes.fromhex("0242f4290001")
ETH_ARP = bytes.fromhex("0806")
ETH_IP = bytes.fromhex("0800")


class PacketHeader(ctypes.Structure):
    _fields_ = [("seconds", ctypes.c_long), ("useconds", ctypes.c_long),
                ("caplen", ctypes.c_uint), ("length", ctypes.c_uint)]


def checksum(data):
    data += bytes(len(data) % 2)
    total = sum(struct.unpack(f"!{len(data) // 2}H", data))
    while total >> 16:
        total = (total & 0xffff) + (total >> 16)
    return 0xffff - total


def ping(interface, host_mac):
    # Only address the MAC assigned by the diagnostic device tree.
    token = b"disco-linux-" + secrets.token_bytes(8)
    identifier = secrets.randbelow(65536)
    ethernet = BOARD_MAC + host_mac
    arp_addresses = host_mac + HOST_IP + BOARD_MAC + BOARD_IP

    def arp(op):
        return ethernet + ETH_ARP + struct.pack("!HHBBH", 1, 2048, 6, 4, op) + arp_addresses

    def echo_request(seq):
        icmp = struct.pack("!BBHHH", 8, 0, 0, identifier, seq) + token
        icmp = icmp[:2] + struct.pack("!H", checksum(icmp)) + icmp[4:]
        ip = struct.pack("!BBHHHBBH4s4s", 0x45, 0, 20 + len(icmp), seq, 0, 64, 1, 0,
                         HOST_IP, BOARD_IP)
        ip = ip[:10] + struct.pack("!H", checksum(ip)) + ip[12:]
        return ethernet + ETH_IP + ip + icmp

    directory = os.path.join(os.environ["SystemRoot"], "System32", "Npcap")
    with os.add_dll_directory(directory):
        pcap = ctypes.CDLL(os.path.join(directory, "wpcap.dll"))

    ptr = ctypes.c_void_p
    string = ctypes.c_char_p
    integer = ctypes.c_int
    def bind(name, result, *arguments):
        function = getattr(pcap, name)
        function.restype = result
        function.argtypes = arguments
        return function

    open_live = bind("pcap_open_live", ptr, string, integer, integer, integer, string)
    nonblock = bind("pcap_setnonblock", integer, ptr, integer, string)
    send = bind("pcap_sendpacket", integer, ptr, string, integer)
    receive = bind("pcap_next_ex", integer, ptr, ctypes.POINTER(ctypes.POINTER(PacketHeader)),
                   ctypes.POINTER(ptr))
    close = bind("pcap_close", None, ptr)
    err = ctypes.create_string_buffer(256)
    handle = open_live(interface.encode(), 65535, 0, 30, err)
    if not handle:
        raise OSError(err.value)

    def emit(frame):
        frame = frame.ljust(60, b"\0")
        if send(handle, frame, len(frame)):
            raise OSError("Npcap send failed")

    sent = set()
    replies = set()
    header = ctypes.POINTER(PacketHeader)()
    packet = ptr()
    try:
        if nonblock(handle, 1, err):
            raise OSError(err.value)
        started = time.monotonic()
        while time.monotonic() - started < 7 and len(replies) < 3:
            seq = int(time.monotonic() - started) + 1
            if seq not in sent:
                emit(arp(1))
                emit(echo_request(seq))
                sent.add(seq)

            result = receive(handle, ctypes.byref(header), ctypes.byref(packet))
            if result < 0:
                raise OSError("Npcap capture failed")
            if result == 0:
                time.sleep(.003)
                continue
            frame = ctypes.string_at(packet, header.contents.caplen)
            if len(frame) < 42 or frame[6:12] != BOARD_MAC:
                continue
            if frame[12:14] == ETH_ARP:
                if frame[20:22] == b"\0\1" and frame[28:32] == BOARD_IP and frame[38:42] == HOST_IP:
                    emit(arp(2))
                continue
            if (frame[:6] != host_mac or frame[12:14] != ETH_IP or frame[23] != 1
                    or frame[26:34] != BOARD_IP + HOST_IP):
                continue
            offset = 14 + (frame[14] & 15) * 4
            icmp = frame[offset:14 + int.from_bytes(frame[16:18], "big")]
            if len(icmp) != 8 + len(token) or icmp[:2] != bytes(2) or icmp[8:] != token or checksum(icmp):
                continue
            ident, seq = struct.unpack("!HH", icmp[4:8])
            if ident == identifier and seq in sent and seq not in replies:
                print(f"198.18.42.2 ICMP Echo Reply seq={seq}")
                replies.add(seq)
    finally:
        close(handle)
    return len(replies) == 3


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("interface", help="Npcap device path for the board-facing interface")
    parser.add_argument("mac", help="physical address of that Windows interface")
    options = parser.parse_args()
    mac = bytes.fromhex(options.mac.replace("-", "").replace(":", ""))
    if len(mac) != 6:
        parser.error("expected a six-byte MAC address")
    raise SystemExit(0 if ping(options.interface, mac) else 1)
