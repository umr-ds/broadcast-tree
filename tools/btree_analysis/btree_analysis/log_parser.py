#! /usr/bin/env python3

import io
import re
import datetime
import sys
import pathlib


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

    return line_dict


def find_node_parent(f: io.TextIOBase) -> str:
    parent = ""

    for line in f:
        parsed_line = parse_line(line)

    return parent


def parse_experiment(experiment_path: str) -> None:
    path = pathlib.Path(experiment_path)

    for file in path.glob('*.log'):
        with open(file, "r") as f:
            find_node_parent(f)


if __name__ == "__main__":
    parse_experiment(sys.argv[1])
