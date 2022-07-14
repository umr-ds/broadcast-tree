#! /usr/bin/env python3

import re
import datetime
import sys
import pathlib
import graphviz

from typing import Any, TextIO
from copy import deepcopy

import toml

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
                try:
                    key, value = arg.strip().split(": ")
                except ValueError:
                    continue
                arguments[key.strip()] = smartcast(value)
            line_dict["arguments"] = arguments
    elif address := IP_REGEX.match(line):
        line_dict["ip_address"] = address.group(0)
        line_dict["message"] = "ip_address"

    return line_dict


def parse_node(
    f: TextIO, node_id: int
) -> tuple[list[dict[str, str | int]] | None, int]:
    events: list[dict[str, str | int | datetime.datetime]] = []
    switch_count = 0
    start_time: datetime.datetime | None = None

    mac_wifi_lookup = transform_metadata(metadata=metadata, key="MAC_WIFI")

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
            events.append(
                {"event": "start", "timestamp": start_time, "node_id": node_id}
            )

        # TODO: Maybe first log that request is confirmed before disconnecting from old parent
        # This would require storing potentially multiple parents at this point.
        if parsed_line["message"] == "Parent confirmed our request.":
            events.append(
                {
                    "event": "connect",
                    "parent": mac_wifi_lookup[parsed_line["arguments"]["addr"]]["ID"],
                    "tx_pwr": 0,
                    "timestamp": parsed_line["timestamp"],
                    "node_id": node_id,
                }
            )
            switch_count += 1

        if (
            parsed_line["message"] == "Disconnected from parent."
            or parsed_line["message"]
            == "Received disconnection command from our parent."
        ):
            events.append(
                {
                    "event": "disconnect",
                    "timestamp": parsed_line["timestamp"],
                    "node_id": node_id,
                }
            )

        if parsed_line["message"] == "Updated self.":
            for event in reversed(events):
                if event["event"] == "connect":
                    event["tx_pwr"] = parsed_line["arguments"]["own_prw"]
                    break

        if (
            parsed_line["message"] == "Received entire payload."
            and "file path" in parsed_line["arguments"]
        ):
            events.append(
                {
                    "event": "receive",
                    "timestamp": parsed_line["timestamp"],
                    "node_id": node_id,
                }
            )

        if parsed_line["level"] in ["WARN", "ERROR", "FATAL"]:
            if parsed_line["message"] == "Payload frame expired.":
                continue
            events.append(
                {
                    "event": "error",
                    "timestamp": parsed_line["timestamp"],
                    "node_id": node_id,
                    "level": parsed_line["level"],
                    "message": parsed_line["message"],
                }
            )

        if parsed_line["message"] == "Ending game.":
            events.append(
                {
                    "event": "finish",
                    "timestamp": parsed_line["timestamp"],
                    "node_id": node_id,
                }
            )

    return events, switch_count


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
    experiment_path: pathlib.Path,
) -> ([{str, str | int}], [int]):
    # build lookup-table for mac-addresses
    mac_eth_lookup = transform_metadata(metadata=metadata, key="MAC_WIFI")

    log_files = experiment_path.glob("*.log")

    all_events: list[dict[str, str | int]] = []

    all_nodes: list[int] = []

    for file in log_files:
        node_name = file.name.split(".")[0].split("-")[1:]
        node_address = f'b8:27:eb:{":".join(node_name)}'
        node_id: int = mac_eth_lookup[node_address]["ID"]
        all_nodes.append(node_id)

        with open(file, "r") as f:
            events, switches = parse_node(f, node_id=node_id)
            if events is None:
                print(experiment_path)
                continue

            print(
                f"Node {node_id} switched parents {switches} times",
                flush=True,
            )

            all_events += events

    all_events.sort(key=lambda event: event["timestamp"])

    return all_events, all_nodes


floor_mapping = {
    0: "box",
    1: "oval",
    2: "triangle",
    3: "diamond",
}


error_colours = {
    "WARN": "yellow",
    "ERROR": "orange",
    "FATAL": "red",
}


def place_node_core(node_id, nodes):
    for node in nodes:
        if node.id == node_id:
            break

    node_room = node.room
    core = node_room[0]
    floor = floor_mapping[int(node_room[1])]

    return floor, core


def build_graph_series(
    events: [{str, str | int}], nodes: [int]
) -> list[dict[str, dict[str | int, str | int]]]:
    graph = {"nodes": {}, "edges": {}, "metadata": {}}
    print("Building graph series")

    graph_series = []

    for event in events:
        graph = deepcopy(graph)

        if event["event"] == "start":
            graph["nodes"][event["node_id"]] = {
                "receive": "-",
                "finish": "-",
                "error": "white",
            }

        if event["event"] == "connect":
            graph["edges"][event["node_id"]] = (event["parent"], event["tx_pwr"])

        if event["event"] == "disconnect":
            del graph["edges"][event["node_id"]]

        if event["event"] == "receive":
            graph["nodes"][event["node_id"]]["receive"] = "R"

        if event["event"] == "finish":
            graph["nodes"][event["node_id"]]["finish"] = "E"

        if event["event"] == "error":
            graph["nodes"][event["node_id"]]["error"] = error_colours[event["level"]]
            graph["nodes"][event["node_id"]]["message"] = event["message"]

        graph["metadata"]["node_id"] = event["node_id"]
        graph["metadata"]["timestamp"] = event["timestamp"]
        graph["metadata"]["type"] = event["event"]

        graph_series.append(graph)

    return graph_series


def graph_to_string(
    graph: dict[str, dict[str | int, str | int]], config: dict[str, Any], cluster: bool
) -> [str]:
    all_nodes = tb_api.get_nodes()

    lines = {
        "A": ["subgraph cluster_A {\nlabel=A\n"],
        "B": ["subgraph cluster_B {\nlabel=B\n"],
        "C": ["subgraph cluster_C {\nlabel=C\n"],
        "D": ["subgraph cluster_D {\nlabel=D\n"],
        "E": ["subgraph cluster_E {\nlabel=E\n"],
        "root": [""],
    }

    last_nodes = {
        "A": None,
        "B": None,
        "C": None,
        "D": None,
        "E": None,
    }

    for node, attributes in graph["nodes"].items():
        shape, core = place_node_core(node, all_nodes)

        color = attributes["error"]

        if node == config["source_id"]:
            color = "green"

        lines[core].append(
            f'{node} [tooltip="{attributes.get("message", "")}", style="filled", fillcolor="{color}", shape="{shape}", label="{node}: {attributes["finish"]}/{attributes["receive"]}"]'
        )

        last_nodes[core] = node

    for node, (parent, tx_pwr) in graph["edges"].items():
        lines["root"].append(f"{node} -> {parent} [label={tx_pwr}]")

    if cluster:
        if last_nodes["A"] is not None and last_nodes["B"] is not None:
            lines["root"].append(
                f"{last_nodes['A']} -> {last_nodes['B']} [ltail=cluster_A, lhead=cluster_B, style=invis]"
            )

        if last_nodes["B"] is not None and last_nodes["C"] is not None:
            lines["root"].append(
                f"{last_nodes['B']} -> {last_nodes['C']} [ltail=cluster_B, lhead=cluster_C, style=invis]"
            )

        if last_nodes["C"] is not None and last_nodes["D"] is not None:
            lines["root"].append(
                f"{last_nodes['C']} -> {last_nodes['D']} [ltail=cluster_C, lhead=cluster_D, style=invis]"
            )

        if last_nodes["D"] is not None and last_nodes["E"] is not None:
            lines["root"].append(
                f"{last_nodes['D']} -> {last_nodes['E']} [ltail=cluster_D, lhead=cluster_E, style=invis]"
            )

        lines = {k: "".join(v + ["}"]) for k, v in lines.items()}

    else:
        lines = {k: "\n".join(v[1:]) for k, v in lines.items()}

    return lines.values() if cluster else list(lines.values()) + ["}"]


def write_graph_series(
    graph_series: list[dict[str, dict[str | int, str | int]]],
    config: dict[str, Any],
    experiment_path: pathlib.Path,
):
    dots = ""

    step_counter = 1
    for graph in graph_series:
        dots += "`digraph {\n"
        dots += 'rankdir="TB"\n'
        dots += f'label="{step_counter}, {graph["metadata"]["timestamp"]}, {graph["metadata"]["type"]}, {graph["metadata"]["node_id"]}"\n'
        dots += "\n".join(graph_to_string(graph, config, False)) + "\n"
        dots += "`,\n"

        step_counter += 1

    with open("graph_animation_template.html", "r") as f:
        template = f.read()
        animation = template.replace("{graph_series}", dots)

    with open(experiment_path / "graph_animation.html", "w") as f:
        f.write(animation)


def usage():
    print(
        "Usage: ./log_parser.py [-s <experiment_path>] [-d <experiment_root_path>]",
        flush=True,
    )


def stats(events):
    received = 0
    ended = 0
    for event in events:
        if event["event"] == "receive":
            received += 1
        if event["event"] == "finish":
            ended += 1

    print(f"Received: {received}, ended {ended}")


if __name__ == "__main__":

    if len(sys.argv) != 3:
        usage()
        exit(1)

    if sys.argv[1] == "-s":
        experiment_path = pathlib.Path(sys.argv[2])

        if pathlib.Path(experiment_path / "graph_animation.html").is_file():
            print("Graph animation already exists")
            exit(0)

        # parse experiment config
        with open(experiment_path / "config", "r") as f:
            config = toml.load(f)

        events, nodes = parse_experiment(experiment_path)
        stats(events)

        graph_series = build_graph_series(events=events, nodes=nodes)

        write_graph_series(graph_series, config, experiment_path)

    elif sys.argv[1] == "-d":
        experiment_root_path = pathlib.Path(sys.argv[2])

        for experiment_path in experiment_root_path.glob("*"):
            print(f"#### Parsing experiment {experiment_path.name}", flush=True)
            if pathlib.Path(experiment_path / "graph_animation.html").is_file():
                print("Graph animation already exists")
                continue

            if (
                ".DS_Store" in experiment_path.parts
                or ".gitkeep" in experiment_path.parts
            ):
                continue

            # parse experiment config
            experiment_path = pathlib.Path(sys.argv[2])

            # parse experiment config
            with open(experiment_path / "config", "r") as f:
                config = toml.load(f)

            events, nodes = parse_experiment(experiment_path)
            stats(events)

            graph_series = build_graph_series(events=events, nodes=nodes)

            write_graph_series(graph_series, config, experiment_path)
    else:
        usage()
