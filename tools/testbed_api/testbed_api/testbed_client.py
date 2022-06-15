#! /usr/bin/env python3

##################################################################
##################################################################
# TODO: THIS NEEDS TO BE FIXED AS SOON AS TESTBED API IS STABLED #
##################################################################
##################################################################

import argparse
import logging
import socket
import _thread
import sys
import time

from threading import Semaphore

import testbed_api.api as api

BATCH_SIZE = 10
ALL_NODES_COUNT = 84
sem = Semaphore(1)


def listen_boot_sock(nodes_to_boot):
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        boot_acks = 0
        sock.bind(("0.0.0.0", 35039))
        sock.listen()

        while boot_acks < nodes_to_boot:
            sock.settimeout(60)
            boot_acks += 1
            print("Waiting for connection")
            try:
                conn, addr = sock.accept()
            except socket.timeout:
                logging.warning("Time out!")
                sem.release()
                continue

            with conn:
                print(f"Connected by {addr}")
                conn.close()
                sem.release()

        sock.shutdown(socket.SHUT_RDWR)


def graceful_boot_nodes(nodes=None, reboot=False):
    if not nodes:
        nodes = api.get_nodes()

    logging.info(f"Gracefully booting {len(nodes)} nodes")

    _ = _thread.start_new_thread(listen_boot_sock, (len(nodes),))

    for node in nodes:
        sem.acquire()
        api.enable_port(node.id)
        if reboot:
            api.reboot_node(node.id)
        else:
            api.boot_node(node.id)

    logging.info("Just some additional waiting. For good measure.")


def get_nodes_by_filter(**kwargs):
    nodes = api.get_nodes()

    for key in kwargs:
        if kwargs[key] is None:
            continue

        if key == "id":
            nodes = [node for node in nodes if node.id in kwargs[key]]
        else:
            nodes = [node for node in nodes if getattr(node, key) == kwargs[key]]

    return nodes


def filter_commands(args):
    filter_dict = {
        "id": args.id,
        "floor": args.floor,
        "room": args.room,
        "switch": args.switch,
        "note": args.note,
        "power": args.power,
    }

    print(get_nodes_by_filter(**filter_dict))


def _cli_get(args: argparse.Namespace) -> None:
    if args.all:
        print(api.get_nodes())
    elif args.node:
        print(api.get_node(args.node))
    else:
        print("Provide node id or use switch to get all", file=sys.stderr, flush=True)
        sys.exit(1)


def _cli_boot(args: argparse.Namespace) -> None:
    if args.graceful:
        graceful_boot_nodes()
    elif args.all:
        api.boot_nodes()
    elif args.node:
        api.boot_node(args.node)
    else:
        print("Provide node id or use switch to boot all", file=sys.stderr, flush=True)
        sys.exit(1)


def _cli_shutdown(args: argparse.Namespace) -> None:
    if args.all:
        api.shutdown_nodes()
    elif args.node:
        api.shutdown_node(args.node)
    else:
        print(
            "Provide node id or use switch to shutdown all", file=sys.stderr, flush=True
        )
        sys.exit(1)


def _cli_reboot(args: argparse.Namespace) -> None:
    if args.graceful:
        print("Shutting down nodes")
        api.shutdown_nodes()
        print("Waiting for nodes to shut down")
        time.sleep(30)
        print("Booting all nodes gracefully")
        graceful_boot_nodes()
    elif args.all:
        api.reboot_nodes()
    elif args.node:
        api.reboot_node(args.node)
    else:
        print(
            "Provide node id or use switch to reboot all", file=sys.stderr, flush=True
        )
        sys.exit(1)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Orchestrate piloty testbed")
    parser.add_argument("-l", "--log", default="INFO", help="Set log level")

    subparsers = parser.add_subparsers()

    command_get = subparsers.add_parser("get", help="Get information on nodes")
    command_get.add_argument(
        "-a", "--all", action="store_true", help="Get list of all nodes"
    )
    command_get.add_argument("node", nargs="?", help="Node ID")
    command_get.set_defaults(func=_cli_get)

    command_boot = subparsers.add_parser("boot", help="Boot nodes")
    command_boot.add_argument("-a", "--all", action="store_true", help="Boot all nodes")
    command_boot.add_argument(
        "-g", "--graceful", action="store_true", help="Boot all nodes gracefully"
    )
    command_boot.add_argument("node", nargs="?", help="Node ID")
    command_boot.set_defaults(func=_cli_boot)

    command_shutdown = subparsers.add_parser("shutdown", help="Shutdown nodes")
    command_shutdown.add_argument(
        "-a", "--all", action="store_true", help="Shutdown all nodes"
    )
    command_shutdown.add_argument("node", nargs="?", help="Node ID")
    command_shutdown.set_defaults(func=_cli_shutdown)

    command_reboot = subparsers.add_parser("reboot", help="Reboot nodes")
    command_reboot.add_argument(
        "-a", "--all", action="store_true", help="Reboot all nodes"
    )
    command_reboot.add_argument(
        "-g", "--graceful", action="store_true", help="Reboot all nodes gracefully"
    )
    command_reboot.add_argument("node", nargs="?", help="Node ID")
    command_reboot.set_defaults(func=_cli_reboot)

    filter_parser = subparsers.add_parser("filter", help="Filter nodes by attribute")
    filter_parser.add_argument(
        "-i", "--id", nargs="+", type=int, help="Explicit list of node ids"
    )
    filter_parser.add_argument("-f", "--floor", help="Floor the nodes are deployed on")
    filter_parser.add_argument("-r", "--room", help="Room the nodes are deployed in")
    filter_parser.add_argument("-s", "--switch", help="Filter by switch")
    filter_parser.add_argument("-n", "--note", help='Filter by note (e.g."OK"')
    filter_parser.add_argument("-p", "--power", type=int, help="Filter by power state")
    filter_parser.set_defaults(func=filter_commands)

    args = parser.parse_args()

    log_level = logging.getLevelName(args.log.upper())
    logging.basicConfig(level=log_level)

    args.func(args)
