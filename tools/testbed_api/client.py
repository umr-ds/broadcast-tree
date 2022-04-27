#! /usr/bin/env python3

import time
import argparse
import subprocess
import json
import logging
import socket
import _thread

from threading import Semaphore
from typing import List, Dict

import api

BATCH_SIZE = 10
ALL_NODES_COUNT = 84
sem = Semaphore(10)


def listen_boot_sock():
    boot_acks = 0
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.bind(("172.23.42.1", 35039))
    sock.listen()

    while boot_acks < ALL_NODES_COUNT:
        print("Waiting for connection")
        conn, addr = sock.accept()
        with conn:
            boot_acks += 1
            print(f"Connected by {addr}")
            conn.close()
            sem.release()


def get_neighbors() -> List[Dict]:
    logging.info("Requesting all neigbors")
    command = ["ip", "--json", "neighbor"]

    ip_result = subprocess.run(command, capture_output=True)
    if ip_result.returncode:
        raise Exception(f"Could not check for neighbors: {ip_result.stderr}")

    return json.loads(str(ip_result.stdout, encoding="utf-8"))


def graceful_boot_nodes(nodes=[], reboot=False):
    boot_listener = _thread.start_new_thread(listen_boot_sock, ())

    if len(nodes) == 0:
        nodes = api.get_nodes()

    logging.info(f"Gracefully booting {len(nodes)} nodes")

    counter = 0
    for node in nodes:
        sem.acquire()
        api.enable_port(node.id)
        if reboot:
            api.reboot_node(node.id)
        else:
            api.boot_node(node.id)


def graceful_shutdown_all():
    logging.info("Shutting down all nodes")
    nodes = api.get_nodes()

    for node in nodes:
        api.shutdown_node(node.id)


def enable_all_ports():
    logging.info("Enabling all ports")
    nodes = api.get_nodes()

    for node in nodes:
        api.enable_port(node.id)


def api_commands(args):
    if args.get_nodes:
        print(api.get_nodes())
    elif args.enable_ports:
        api.enable_ports()
    elif args.disable_ports:
        api.disable_ports()
    elif args.reboot_nodes:
        api.reboot_nodes()
    elif args.get_node:
        print(api.get_node(args.get_node))
    elif args.enable_port:
        api.enable_port(args.enable_port)
    elif args.disable_port:
        api.disable_port(args.disable_port)
    elif args.shutdown_node:
        api.shutdown_node(args.shutdown_node)
    elif args.reboot_node:
        api.reboot_node(args.reboot_node)
    elif args.boot_node:
        api.boot_node(args.boot_node)


def client_commands(args):
    if args.boot_nodes:
        graceful_boot_nodes()
    if args.shutdown_nodes:
        graceful_shutdown_all()
    if args.enable_ports:
        enable_all_ports()


if __name__ == "__main__":
    logging.basicConfig(level=logging.DEBUG)

    parser = argparse.ArgumentParser(description="Orchestrate piloty testbed")
    subparsers = parser.add_subparsers()

    # Create the parser for simple API commands
    parser_api = subparsers.add_parser("api", help="Commands for native API calls")
    parser_api.add_argument(
        "-n", "--get_nodes", action="store_true", help="Get all nodes"
    )
    parser_api.add_argument(
        "-e", "--enable_ports", action="store_true", help="Enable all ports"
    )
    parser_api.add_argument(
        "-d", "--disable_ports", action="store_true", help="Disable all ports"
    )
    parser_api.add_argument(
        "-r", "--reboot_nodes", action="store_true", help="Reboot all nodes"
    )

    parser_api.add_argument("-g", "--get_node", metavar="NODE ID", help="Get node")
    parser_api.add_argument(
        "-p", "--enable_port", metavar="NODE ID", help="Enable port"
    )
    parser_api.add_argument(
        "-o", "--disable_port", metavar="NODE ID", help="Disable port"
    )
    parser_api.add_argument("-b", "--boot_node", metavar="NODE ID", help="Boot node")
    parser_api.add_argument(
        "-s", "--shutdown_node", metavar="NODE ID", help="Shut down node"
    )
    parser_api.add_argument(
        "-t", "--reboot_node", metavar="NODE ID", help="Reboot node"
    )
    parser_api.set_defaults(func=api_commands)

    # Create the parser for high-level commands
    parser_client = subparsers.add_parser(
        "client", help="Commands for high-level client commands"
    )
    parser_client.add_argument(
        "-b", "--boot_nodes", action="store_true", help="Gracefully boot all nodes"
    )
    parser_client.add_argument(
        "-s", "--shutdown_nodes", action="store_true", help="Shutdown all nodes"
    )
    parser_client.add_argument(
        "-e", "--enable_ports", action="store_true", help="Enable all ports"
    )
    parser_client.set_defaults(func=client_commands)

    args = parser.parse_args()

    args.func(args)
