#! /usr/bin/env python3

import re
import datetime
import sys
import pathlib
import graphviz

from typing import Any, TextIO

from node_mapping import metadata
from testbed_api import api as tb_api

IP_REGEX = re.compile(r"\d{3}.\d{2}.\d{2}.\d{3}")
TIME_REGEX = re.compile(r"\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}.\d{6}")
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
            timestamp.group(0), "%Y-%m-%d %H:%M:%S.%f"
        )

        line_dict["level"] = LOG_LEVEL_REGEX.search(line).group(0)

        line_dict["sourcefile"] = SOURCEFILE_REGEX.search(line).group(0)[:-1]

        the_rest = " ".join(line.split()[4:]).split("[")

        line_dict["message"] = the_rest[0].strip()

        if line_dict["level"] == "ERROR":
            return line_dict

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


def find_node_parent(f: TextIO) -> tuple[list[dict[str, str | int]] | None, int]:
    parent_data: list[dict[str, str | int | datetime.datetime]] = []
    switch_count = 0
    start_time: datetime.datetime | None = None

    for line in f:
        try:
            parsed_line = parse_line(line)
        except (AttributeError, KeyError):
            return None, 0

        if not parsed_line:
            continue

        # take the first log entry as the start time
        if not start_time:
            start_time = parsed_line["timestamp"]
            parent_data.append({"parent": "", "tx_pwr": 0, "timestamp": start_time})

        if parsed_line["message"] == "Parent confirmed our request.":
            parent_data.append({"parent": parsed_line["arguments"]["addr"], "tx_pwr": 0, "timestamp": parsed_line["timestamp"]})
            switch_count += 1
        if (
            parsed_line["message"] == "Disconnected from parent."
            or parsed_line["message"]
            == "Received disconnection command from our parent."
        ):
            parent_data.append({"parent": "", "tx_pwr": 0, "timestamp": parsed_line["timestamp"]})
        if parsed_line["message"] == "Updated self.":
            parent_data[-1]["tx_pwr"] = parsed_line["arguments"]["own_prw"]
        if (
            parsed_line["message"]
            == "Received entire payload and have no children. Disconnecting from parent."
        ):
            break

    return parent_data, switch_count


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


def get_node_errors(f):
    f.seek(0)
    error_messages = {
        "color": set(),
        "msg": set(),
    }
    for line in f:
        try:
            parsed_line = parse_line(line)
        except (AttributeError, KeyError):
            return None

        if not parsed_line:
            continue

        color = None
        msg = None

        if parsed_line["level"] == "WARN":
            color = "yellow"
            msg = parsed_line["message"]

        if parsed_line["level"] == "ERROR":
            color = "orange"
            msg = parsed_line["message"]

        if parsed_line["message"] == "FATAL":
            color = "red"
            msg = parsed_line["message"]

        if color:
            error_messages["color"].add(color)
            error_messages["msg"].add(msg)
            color = None
            msg = None

    return error_messages


def check_node_reception(f):
    f.seek(0)
    for line in f:
        try:
            parsed_line = parse_line(line)
        except (AttributeError, KeyError):
            return None

        if not parsed_line:
            continue

        if (
            parsed_line["message"] == "Received entire payload."
            and "file path" in parsed_line["arguments"]
        ):
            return "R"

    return "-"


def check_game_fin(f):
    f.seek(0)
    for line in f:
        try:
            parsed_line = parse_line(line)
        except (AttributeError, KeyError):
            return None

        if not parsed_line:
            continue

        if parsed_line["message"] == "Ending game.":
            return "F"

    return "-"


def parse_experiment(
    experiment_path: pathlib.Path,
) -> (dict[str, (str, int)], list[dict[str, str | int]]):

    config = ""
    # parse experiment config
    with open(experiment_path / "config", "r") as f:
        config = ", ".join(f.read().splitlines())

    # build lookup-table for mac-addresses
    mac_lookup = transform_metadata(metadata=metadata, key="MAC_ETH")

    #  build logical representation of the btree
    btree: dict[str, (str, int)] = {}

    log_files = experiment_path.glob("*.log")

    for file in log_files:
        node_name = file.name.split(".")[0].split("-")[1:]
        node_address = f'b8:27:eb:{":".join(node_name)}'

        with open(file, "r") as f:
            parent_data, switches = find_node_parent(f)
            if parent_data is None:
                print(experiment_path)
                continue

            print(
                f"Node {mac_lookup[node_address]['ID']} switched parents {switches} times",
                flush=True,
            )

            if (node_errors := get_node_errors(f)) is None:
                print(experiment_path)
                continue

            if (reception := check_node_reception(f)) is None:
                print(experiment_path)
                continue

            if (ended_game := check_game_fin(f)) is None:
                print(experiment_path)
                continue

            btree[node_address] = (
                parent_data[-1]["parent"],
                parent_data[-1]["tx_pwr"],
                node_errors,
                reception,
                ended_game,
            )

    return btree, metadata, config


floor_mapping = {
    0: "box",
    1: "oval",
    2: "triangle",
    3: "diamond",
}


def place_node_core(node_id, nodes):
    for node in nodes:
        if node.id == node_id:
            break

    node_room = node.room
    core = node_room[0]
    floor = floor_mapping[int(node_room[1])]

    return floor, core


def plot_graph(
    btree: dict[str, str],
    mac_lookup: dict[str, dict[str, str | int]],
    config: str,
    out_path: pathlib.Path,
) -> None:

    cluter_graph = graphviz.Digraph(name="btree")
    regular_graph = graphviz.Digraph(name="btree")

    all_nodes = tb_api.get_nodes()

    unconnected = 0
    not_ended = 0
    not_received = 0
    for node, (parent, parent_tx, errors, reception, ended_game) in btree.items():
        node_color = "lightgrey"
        err_msg = "\n".join(errors["msg"])

        if "yellow" in errors["color"]:
            node_color = "yellow"
        if "orange" in errors["color"]:
            node_color = "orange"
        if "red" in errors["color"]:
            node_color = "red"

        if reception == "-":
            not_received += 1
        if ended_game == "-":
            not_ended += 1

        node_id = mac_lookup[node]["ID"]

        shape, core = place_node_core(node_id, all_nodes)

        with cluter_graph.subgraph(name=f"cluster_{core}") as sg:
            sg.attr(label=core)
            sg.attr(newrank="true")

            sg.node(
                str(node_id),
                label=f"{node_id}: {ended_game}/{reception}",
                style="filled",
                fillcolor=node_color,
                shape=shape,
                # xlabel=f'{node_id}: {err_msg}',
            )

        regular_graph.node(
            str(node_id),
            label=f"{node_id}: {ended_game}/{reception}",
            style="filled",
            fillcolor=node_color,
            shape=shape,
            # xlabel=f'{node_id}: {err_msg}',
        )

        if parent:
            cluter_graph.edge(
                str(node_id),
                str(mac_lookup[parent]["ID"]),
                label=str(parent_tx),
            )

            regular_graph.edge(
                str(node_id),
                str(mac_lookup[parent]["ID"]),
                label=str(parent_tx),
            )
        else:
            unconnected += 1

    run_stats = f"{config}\n{unconnected} unconnected nodes, {not_ended} not ended, {not_received} not received\nBox: -1, Oval: 0, Triangle: 1, Diamond: 2"
    cluter_graph.attr(label=run_stats)
    regular_graph.attr(label=run_stats)
    print(run_stats, flush=True)

    cluter_graph.render(pathlib.Path(out_path / "cluster"))
    regular_graph.render(pathlib.Path(out_path / "tree"))


def usage():
    print(
        "Usage: ./log_parser.py [-s <experiment_path>] [-d <experiment_root_path>]",
        flush=True,
    )


if __name__ == "__main__":

    if len(sys.argv) != 3:
        usage()
        exit(1)

    if sys.argv[1] == "-s":
        experiment_path = pathlib.Path(sys.argv[2])
        btree, metadata, config = parse_experiment(experiment_path)

        # build lookup-table for mac-addresses
        mac_lookup = transform_metadata(metadata=metadata, key="MAC_ETH")

        plot_graph(
            btree=btree, mac_lookup=mac_lookup, config=config, out_path=experiment_path
        )

    elif sys.argv[1] == "-d":
        experiment_root_path = pathlib.Path(sys.argv[2])

        for experiment_path in experiment_root_path.glob("*"):
            print(f"#### Parsing experiment {experiment_path.name}", flush=True)

            if (
                ".DS_Store" in experiment_path.parts
                or ".gitkeep" in experiment_path.parts
            ):
                continue

            btree, metadata, config = parse_experiment(experiment_path)

            # build lookup-table for mac-addresses
            mac_lookup = transform_metadata(metadata=metadata, key="MAC_ETH")

            plot_graph(
                btree=btree,
                mac_lookup=mac_lookup,
                config=config,
                out_path=experiment_path,
            )
    else:
        usage()
