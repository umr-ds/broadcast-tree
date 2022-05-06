#! /usr/bin/env python3

import io
import re
import datetime
import sys
import pathlib


IP_REGEX = re.compile(r"\d{3}.\d{2}.\d{2}.\d{3}")
TIME_REGEX = re.compile(r"\d{4}-\d{2}-\d{2} \d{2}:\d{2}:\d{2}")
LOG_LEVEL_REGEX = re.compile(r"TRACE|DEBUG|INFO|WARN|ERROR|FATAL")
SOURCEFILE_REGEX = re.compile(r"src/.*:\d*:")


def parse_line(line: str) -> dict:
    line_dict = {}
    if timestamp := TIME_REGEX.match(line):
        line_dict["timestamp"] = datetime.datetime.strptime(timestamp.group(0), "%Y-%m-%d %H:%M:%S")

        line_dict["level"] = LOG_LEVEL_REGEX.search(line).group(0)
        line_dict["sourcefile"] = SOURCEFILE_REGEX.search(line).group(0)[:-1]

        the_rest = ' '.join(line.split()[4:]).split("[")

        line_dict["message"] = the_rest[0].strip()

        if len(the_rest) > 1:
            arguments = {}
            if line_dict["message"] == "Current counter:":
                arguments["counter"] = int(the_rest[1][:-1])
            elif line_dict["message"] == "Figured out max sending power.":
                arguments["max_power"] = int(the_rest[1][:-1])
            else:
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


def find_node_parent(f: io.TextIOBase) -> tuple[str, str, int]:
    own_address = ""
    parent = ""
    tx_pwr = 0

    for line in f:
        parsed_line = parse_line(line)
        if parsed_line["message"] == "Parent confirmed our request.":
            parent = parsed_line["arguments"]["addr"]
        if parsed_line["message"] == "Disconnected from parent." or parsed_line["message"] == "Received disconnection command from our parent.":
            parent = ""
        if parsed_line["message"] == "Updated self.":
            tx_pwr = parsed_line["arguments"]["own_prw"]
        if parsed_line["message"] == "ip_address":
            own_address = parsed_line["ip_address"]

    return own_address, parent, tx_pwr


def parse_experiment(experiment_path: str) -> None:
    path = pathlib.Path(experiment_path)

    graph = {}

    for file in path.glob('*.log'):
        node_name = file.name.split(".")[0].split("-")[1:]
        node_address = f'b8:27:eb:{":".join(node_name)}'

        with open(file, "r") as f:
            find_node_parent(f)


if __name__ == "__main__":
    parse_experiment(sys.argv[1])
