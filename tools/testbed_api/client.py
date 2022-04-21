#! /usr/bin/env python3

from nis import match
import time
import argparse
import subprocess
import json

from typing import List, Dict

import api


BATCH_SIZE = 10


def get_neighbors() -> List[Dict]:
    command = ["ip", "--json", "neighbor"]

    ip_result = subprocess.run(command, capture_output=True)
    if ip_result.returncode:
        raise Exception(f"Could not check for neighbors: {ip_result.stderr}")

    return json.loads(str(ip_result.stdout, encoding="utf-8"))


def graceful_boot_nodes(nodes=[], reboot=False):
    if len(nodes) == 0:
        nodes = api.get_nodes()

    counter = 0
    for node in nodes:
        if reboot:
            api.reboot_node(node.id)
        else:
            api.boot_node(node.id)

        if counter == BATCH_SIZE:
            time.sleep(30)
            counter = 0
        else:
            counter += 1

    neighbors = get_neighbors()
    missing_nodes = []
    for node in nodes:
        node_status = [
            neighbor for neighbor in neighbors if neighbor["lladdr"] == node.mac
        ]
        missing = True
        for status in node_status:
            if status["state"][0] == "REACHABLE":
                missing = False
        if missing:
            missing_nodes.append(node)

    if len(missing_nodes) > 0:
        graceful_boot_nodes(missing_nodes, reboot=True)


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


if __name__ == "__main__":
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
    parser_client.set_defaults(func=client_commands)

    args = parser.parse_args()

    args.func(args)
