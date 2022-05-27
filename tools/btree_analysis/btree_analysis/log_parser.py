#! /usr/bin/env python3

import re
import datetime
import sys
import pathlib
import graphviz

from typing import Any, TextIO
from copy import deepcopy

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


def build_graph_series(
    events: [{str, str | int}], nodes: [int]
) -> list[dict[str, dict[str | int, str | int]]]:
    graph = {"nodes": {}, "edges": {}}
    for node in nodes:
        graph["nodes"][node] = {"receive": "-", "finish": "-", "error": "white"}

    graph_series = [graph]

    for event in events:
        graph = deepcopy(graph)

        if event["event"] == "connect":
            graph["edges"][event["node_id"]] = event["parent"]

        if event["event"] == "disconnect":
            del graph["edges"][event["node_id"]]

        if event["event"] == "receive":
            graph["nodes"][event["node_id"]]["receive"] = "R"

        if event["event"] == "finish":
            graph["nodes"][event["node_id"]]["finish"] = "E"

        if event["event"] == "error":
            graph["nodes"][event["node_id"]]["error"] = error_colours[event["level"]]

        graph_series.append(graph)

    return graph_series


def graph_to_string(graph: dict[str, dict[str | int, str | int]]) -> [str]:
    all_nodes = tb_api.get_nodes()

    lines = ["'digraph {'"]

    for node, attributes in graph["nodes"].items():
        shape, _ = place_node_core(node, all_nodes)
        lines.append(f'\'{node} [style="filled", fillcolor="{attributes["error"]}", shape="{shape}", label="{node}: {attributes["finish"]}/{attributes["receive"]}"]\'')

    for node, parent in graph["edges"].items():
        lines.append(f"'{node} -> {parent}'")

    lines.append("'}'")

    return lines


def write_graph_series(graph_series: list[dict[str, dict[str | int, str | int]]]):
    dots = ""

    for graph in graph_series:
        dots += "[\n" + ",\n".join(graph_to_string(graph)) + "],\n"

    with open("graph_animation_template.html", "r") as f:
        template = f.read()
        animation = template.replace("{graph_series}", dots)

    with open("graph_animation.html", "w") as f:
        f.write(animation)


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

        # parse experiment config
        with open(experiment_path / "config", "r") as f:
            config = ", ".join(f.read().splitlines())

        events, nodes = parse_experiment(experiment_path)

        graph_series = build_graph_series(events=events, nodes=nodes)

        write_graph_series(graph_series)

    elif sys.argv[1] == "-d":
        experiment_root_path = pathlib.Path(sys.argv[2])

        for experiment_path in experiment_root_path.glob("*"):
            print(f"#### Parsing experiment {experiment_path.name}", flush=True)

            if (
                ".DS_Store" in experiment_path.parts
                or ".gitkeep" in experiment_path.parts
            ):
                continue

            # parse experiment config
            with open(experiment_path / "config", "r") as f:
                config = ", ".join(f.read().splitlines())

            events, nodes = parse_experiment(experiment_path)
    else:
        usage()
