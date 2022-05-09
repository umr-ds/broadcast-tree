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


def smartcast(value: str) -> str | int:
    value = value.strip()
    try:
        return int(value)
    except ValueError:
        return value


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
                key, value = arg.strip().split(": ")
                arguments[key.strip()] = smartcast(value)
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
        if (
            parsed_line["message"]
            == "Received entire payload and have no children. Disconnecting from parent."
        ):
            break

    parent_data["switch_count"] = switch_count
    return parent_data


def parse_id_file(f: TextIO) -> dict[str, str | int]:
    metadata: dict[str, str | int] = {}

    for line in f:
        key, value = line.strip().split()
        metadata[key[:-1]] = smartcast(value)

    return metadata


def transform_metadata(
    metadata: list[dict[str, str | int]], key: str
) -> dict[str | int, dict[str, str | int]]:
    transformed: dict[str | int, dict[str, str | int]] = {}

    for node_metadata in metadata:
        new_key: str | int = node_metadata[key]
        new_dict: dict[str, str | int] = {
            k: v for (k, v) in node_metadata.items() if k != key
        }
        transformed[new_key] = new_dict

    return transformed


def parse_experiment(
    experiment_path: str,
) -> (dict[str, str], list[dict[str, str | int]]):
    path = pathlib.Path(experiment_path)

    #  parse node_metadata
    metadata: list[dict[str, str | int]] = []
    for file in path.glob("*.ids"):
        node_name = file.name.split(".")[0]
        node_address = f'b8:27:eb:{":".join(node_name.split("-")[1:])}'

        with open(file, "r") as f:
            node_metadata = parse_id_file(f)
            node_metadata["NAME"] = node_name
            node_metadata["MAC_ETH"] = node_address
            metadata.append(node_metadata)

    # build lookup-table for mac-addresses
    mac_lookup = transform_metadata(metadata=metadata, key="MAC_ETH")

    #  build logical representation of the btree
    btree: dict[str, str] = {}

    for file in path.glob("*.log"):
        node_name = file.name.split(".")[0].split("-")[1:]
        node_address = f'b8:27:eb:{":".join(node_name)}'

        with open(file, "r") as f:
            parent_data = find_node_parent(f)
            print(
                f"Node {mac_lookup[node_address]['ID']} switched parents {parent_data['switch_count']} times"
            )
            btree[node_address] = parent_data["parent"]

    return btree, metadata


def plot_graph(
    btree: dict[str, str], mac_lookup: dict[str, dict[str, str | int]], out_path: str
) -> None:
    graph = graphviz.Digraph("btree")

    for node, parent in btree.items():
        graph.node(str(mac_lookup[node]["ID"]))
        if parent:
            graph.edge(str(mac_lookup[node]["ID"]), str(mac_lookup[parent]["ID"]))

    graph_path = pathlib.Path(f"{out_path}/btree")
    graph.render(graph_path)


if __name__ == "__main__":
    btree, metadata = parse_experiment(sys.argv[1])

    # build lookup-table for mac-addresses
    mac_lookup = transform_metadata(metadata=metadata, key="MAC_ETH")

    plot_graph(btree=btree, mac_lookup=mac_lookup, out_path=sys.argv[1])
