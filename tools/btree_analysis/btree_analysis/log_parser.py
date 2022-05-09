#! /usr/bin/env python3

import re
import datetime
import sys
import pathlib
import graphviz

from typing import Any, TextIO


IP_REGEX = re.compile(r"\d{3}.\d{2}.\d{2}.\d{3}")
TIME_REGEX = re.compile(r"\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}")
LOG_LEVEL_REGEX = re.compile(r"TRACE|DEBUG|INFO|WARN|ERROR|FATAL")
SOURCEFILE_REGEX = re.compile(r"src/.*:\d*:")


def parse_line(line: str) -> dict[str, datetime.datetime | str | dict[str, Any]]:
    line_dict = {}
    if timestamp := TIME_REGEX.match(line):
        line_dict["timestamp"] = datetime.datetime.strptime(
            timestamp.group(0), "%Y-%m-%d %H:%M:%S"
        )

        line_dict["level"] = LOG_LEVEL_REGEX.search(line).group(0)
        line_dict["sourcefile"] = SOURCEFILE_REGEX.search(line).group(0)[:-1]

        the_rest = " ".join(line.split()[4:]).split("[")

        line_dict["message"] = the_rest[0].strip()

        if len(the_rest) > 1:
            arguments = {}
            args = the_rest[1][:-1].split(",")
            for arg in args:
                key, value_raw = arg.strip().split(": ")
                try:
                    value = int(value_raw.strip())
                except ValueError:
                    value = value_raw.strip()
                arguments[key.strip()] = value
            line_dict["arguments"] = arguments
    elif address := IP_REGEX.match(line):
        line_dict["ip_address"] = address.group(0)
        line_dict["message"] = "ip_address"

    return line_dict


def find_node_parent(f: TextIO) -> dict[str, str | int]:
    parent_data: dict[str, str | int] = {"parent": ""}
    switch_count = 0

    for line in f:
        parsed_line = parse_line(line)
        if not parsed_line:
            continue

        if parsed_line["message"] == "Parent confirmed our request.":
            parent_data["parent"] = parsed_line["arguments"]["addr"]
            switch_count += 1
        if (
            parsed_line["message"] == "Disconnected from parent."
            or parsed_line["message"]
            == "Received disconnection command from our parent."
        ):
            parent_data["parent"] = ""
        if parsed_line["message"] == "Updated self.":
            parent_data["tx_pwr"] = parsed_line["arguments"]["own_prw"]
        # if parsed_line["message"] == "ip_address":
        #     own_address = parsed_line["ip_address"]
        if (
            parsed_line["message"]
            == "Received entire payload and have no children. Disconnecting from parent."
        ):
            break

    parent_data["switch_count"] = switch_count
    return parent_data


def parse_experiment(experiment_path: str) -> dict[str, str]:
    path = pathlib.Path(experiment_path)

    btree: dict[str, str] = {}

    for file in path.glob("*.log"):
        node_name = file.name.split(".")[0].split("-")[1:]
        node_address = f'b8:27:eb:{":".join(node_name)}'

        with open(file, "r") as f:
            parent_data = find_node_parent(f)
            print(
                f"Node {node_address} switched parents {parent_data['switch_count']} times"
            )
            btree[node_address] = parent_data["parent"]

    return btree


def plot_graph(btree: dict[str, str], out_path: str) -> None:
    graph = graphviz.Digraph("btree")

    for node, parent in btree.items():
        graph.node(node)
        if parent:
            graph.edge(node, parent)

    print(graph.source)

    # graph_path = pathlib.Path(f"{out_path}/graphs")
    # graph.render(graph_path)


if __name__ == "__main__":
    btree = parse_experiment(sys.argv[1])
    plot_graph(btree=btree, out_path=sys.argv[1])
