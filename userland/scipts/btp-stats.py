#!/usr/bin/python3

# Listens for UDPs containing stats send from wifi chip

import socket
import threading
import sys
import struct

UDP_IP = "0.0.0.0"
UDP_PORT = 5500

labels = [
    "tx duration in µs",
    "rx duration in µs overall",
    "rx duration in µs current bss",
    "rx duration in µs other bss",
    "802.11 of unknown type",
    "non 802.11",
    "tx slot counter",
    "tx slot time",
    "µs good tx",
    "µs bad tx",
    "num tx",
    "num tx RTS",
    "num tx CTS",
    "num tx ACK",
    "num tx DNL",
    "num tx BCN",
    "num tx AMPDU",
    "num tx MPDU",
    "tx unicast",
    "num rx",
    "num rx/carriersense glitches",
    "num bad PLCP",
    "frame id",
]

laststats = [0] * 23

counter = 0


def print_stats(data, lock, cnt):
    with lock:
        global counter
        print("stats frame:", cnt)
        counter += 1
        j = 0
        for i in struct.iter_unpack("<I", data):
            i = i[0]
            delta = i - laststats[j]
            laststats[j] = i
            print("%35s %12d (diff %12d)" % (labels[j], i, delta))
            j = j + 1
        print("---------------------------------------")
        exit()


stop_listener = False


def listen():
    lock = threading.Lock()
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.bind((UDP_IP, UDP_PORT))
    print("* listening on " + UDP_IP + ":" + str(UDP_PORT))
    global stop_listener
    global counter
    while True:
        data, addr = sock.recvfrom(1024)
        if stop_listener:
            sock.close()
            exit()
        plotter = threading.Thread(target=print_stats, args=(data, lock, counter))
        plotter.start()


def main():
    listener = threading.Thread(target=listen)
    listener.deamon = True
    listener.start()
    try:
        input("Press enter to exit\n")
    except SyntaxError:
        pass
    global stop_listener
    stop_listener = True
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.sendto(b"exit", (UDP_IP, UDP_PORT))
    sock.close()
    listener.join()
    sys.exit()


if __name__ == "__main__":
    main()
