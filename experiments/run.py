#! /usr/bin/env python3

import argparse
import time
import datetime
import os

import toml

from pssh.clients import ParallelSSHClient, SSHClient

import testbed_api.client as client


def run(node_filter, source_id, experiment_config, iteration):
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
    iface = "$(grep -l b8:27 /sys/class/net/wlan*/address | cut -d'/' -f5)"
    logfile_path = f"{logfile_path_base}/$(hostname).log"

    btp_client_cmd = (
        f'bash -c "nohup btp --log_file={logfile_path} --log_level=2 {max_power} {iface} > /dev/null 2> {logfile_path}.err &"'
    )
    client_output = client_nodes.run_command(btp_client_cmd)
    for host_out in client_output:
        print(f"{host_out.host} started")

    print("-> Starting source node")
    source.run_command(
        f"dd bs={experiment_config['payload_size']} count=1 </dev/urandom > source.file"
    )

    print("-> Starting experiment")
    max_power = "--max_power" if experiment_config["max_power"] else ""
    iface = "$(grep -l b8:27 /sys/class/net/wlan*/address | cut -d'/' -f5)"
    source_logfile_path = f"{logfile_path_base}/source_$(hostname).log"

    btp_client_cmd = f'bash -c "nohup btp --source=source.file --log_file={source_logfile_path} --log_level=2 {max_power} {iface} > /dev/null 2> {source_logfile_path}.err &"'
    source.run_command(btp_client_cmd)

    print("-> Waiting for experiment to finish")
    time.sleep(10)

    print("-> Stopping BTP on all nodes")
    source.run_command("pkill btp")
    client_nodes.run_command("pkill btp")

    source.run_command(f'ip address show dev eth0 | egrep -o "172.23.42.1[0-9]{{2,}}" >> {source_logfile_path}')
    client_nodes.run_command(f'ip address show dev eth0 | egrep -o "172.23.42.1[0-9]{{2,}}" >> {logfile_path}')

    print("-> Collecting logs and cleaning up")
    source.copy_remote_file(
        f"{logfile_path_base}",
        f"{os.getcwd()}/results/{experiment_time}",
        recurse=True,
    )
    source.run_command(f"rm -rf {logfile_path_base}")

    conf_file = open(f"{os.getcwd()}/results/{experiment_time}/config", "w")
    conf_file.write(
        f"Used clients:\n{pssh_nodes}\n\nSource:\n172.23.42.{source_node.id + 100}\n\nConfig:\n{experiment_config}\n\nIteration:\n{iteration}"
    )
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
            conf["CLIENTS"],
            conf["SOURCE"]["id"],
            conf["EXPERIMENT"],
            iteration,
        )
