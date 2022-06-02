#! /usr/bin/env python3

import argparse
import time
import datetime
import os

import toml

from pssh.clients import ParallelSSHClient, SSHClient

import testbed_api.client as client


def run(conf, iteration):
    node_filter = conf["CLIENTS"]
    source_id = conf["SOURCE"]["id"]
    experiment_config = conf["EXPERIMENT"]

    print("Running prepare stage")

    _nodes = client.get_nodes_by_filter(**node_filter)
    client_nodes = [node for node in _nodes if node.id != source_id]
    source_node = client.get_nodes_by_filter(**{"id": [source_id]})[0]

    print("-> Generating log file path")
    experiment_time = datetime.datetime.now().strftime("%Y-%m-%d_%H-%M-%S")
    logfile_path_base = f"/btree_data/{experiment_time}"

    print("Running run stage")

    print("Preparing log folder on testbed contoller")
    source = SSHClient(
        f"172.23.42.{source_node.id + 100}",
        user="root",
        password="raspberry",
        allow_agent=False,
    )
    source.run_command(f"mkdir {logfile_path_base}")

    print("Preparing hosts for PSSH")
    pssh_nodes = [f"172.23.42.{node.id + 100}" for node in client_nodes]

    client_nodes = ParallelSSHClient(
        pssh_nodes, user="root", password="raspberry", allow_agent=False
    )

    print("-> Starting client nodes")
    max_power = "--max_power" if experiment_config["max_power"] else ""

    poll_timeout = f'--poll_timeout={experiment_config["poll_timeout"]}'
    discovery_bcast_interval = (
        f'--broadcast_timeout={experiment_config["discovery_bcast_interval"]}'
    )
    pending_timeout = f'--pending_timeout={experiment_config["pending_timeout"]}'
    source_retransmit_payload = (
        f'--retransmit_timeout={experiment_config["source_retransmit_payload"]}'
    )
    unchanged_counter = f'--unchanged_counter={experiment_config["unchanged_counter"]}'
    omit_roll_back = "--omit_roll_back" if experiment_config["omit_roll_back"] else ""

    iface = "$(grep -l b8:27 /sys/class/net/wlan*/address | cut -d'/' -f5)"

    client_logfile_path = f"{logfile_path_base}/$(hostname).log"
    source_logfile_path = f"{logfile_path_base}/source_$(hostname).log"

    btp_cmd_common = (
        f'bash -c "nohup btp --log_level=2'
        f" {{0}}"
        f" {max_power} {omit_roll_back} {unchanged_counter}"
        f" {poll_timeout} {discovery_bcast_interval} {pending_timeout} {source_retransmit_payload}"
        f' --log_file={{1}} {iface} > /dev/null 2> {{1}}.err &"'
    )

    print("Stopping potential zombie BTP processes")
    source.run_command("pkill btp")
    client_nodes.run_command("pkill btp")

    client_output = client_nodes.run_command(
        btp_cmd_common.format("", client_logfile_path)
    )
    for host_out in client_output:
        print(f"{host_out.host} started")

    print("-> Starting source node")
    source.run_command(
        f"dd bs={experiment_config['payload_size']} count=1 </dev/urandom > source.file"
    )

    print("-> Starting experiment")
    source_logfile_path = f"{logfile_path_base}/source_$(hostname).log"

    source.run_command(
        btp_cmd_common.format("--source=source.file", source_logfile_path)
    )

    print("-> Waiting for experiment to finish")
    time.sleep(60)

    print("-> Stopping BTP on all nodes")
    source.run_command("pkill btp")
    client_nodes.run_command("pkill btp")

    print("-> Collecting logs and cleaning up")
    source.copy_remote_file(
        f"{logfile_path_base}",
        f"{os.getcwd()}/results/{experiment_time}",
        recurse=True,
    )
    source.run_command(f"rm -rf {logfile_path_base}")

    conf_file = open(f"{os.getcwd()}/results/{experiment_time}/config", "w")
    toml.dump(conf, conf_file)
    conf_file.close()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Run broadcast tree experiments on Piloty testbed"
    )

    parser.add_argument(
        "-c", "--config", help="Experiment configuration", required=True
    )

    args = parser.parse_args()

    conf = toml.load(args.config)
    iterations = conf["EXPERIMENT"]["iterations"]
    for iteration in range(iterations):
        print(f"Running iteration {iteration + 1} out of {iterations}")
        run(
            conf,
            iteration,
        )
