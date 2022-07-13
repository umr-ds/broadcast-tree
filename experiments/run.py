#! /usr/bin/env python3

import argparse
import time
import datetime
import os
import subprocess

from typing import Any

import toml
import testbed_api.testbed_client as client
import testbed_api.api as tb_api

from pssh.clients import ParallelSSHClient, SSHClient


def ping(host):
    command = ["ping", "-c", "1", host]
    return subprocess.call(command, stdout=subprocess.DEVNULL) == 0


def fix_testbed(clients, source) -> None:
    all_up_and_running = True

    if not ping(f"172.23.42.{100 + source.id}"):
        print(f"Source node ({source.id}) is down.")
        tb_api.shutdown_node(source.id)
        time.sleep(1)
        tb_api.enable_port(source.id)
        time.sleep(1)
        tb_api.boot_node(source.id)
        time.sleep(1)
        all_up_and_running = False

    for client in clients:
        if not ping(f"172.23.42.{100 + client.id}"):
            print(f"Node {client.id} is down.")
            tb_api.shutdown_node(client.id)
            time.sleep(1)
            tb_api.enable_port(client.id)
            time.sleep(1)
            tb_api.boot_node(client.id)
            time.sleep(1)
            all_up_and_running = False

    return all_up_and_running


def experiment_already_done(experiment_config):
    results_path = f"{os.getcwd()}/results/"
    for entry in os.listdir(results_path):
        if os.path.isdir(f"{results_path}{entry}"):
            finished_experiment_config = toml.load(f"{results_path}{entry}/config")

            if finished_experiment_config == experiment_config:
                return True

    return False


def run(conf):
    if experiment_already_done(conf):
        print("# -> Experiment already done.")
        return

    _nodes = client.get_nodes_by_filter(**node_filter)
    client_nodes = [node for node in _nodes if node.id != source_id]
    source_node = client.get_nodes_by_filter(**{"id": [source_id]})[0]

    if not fix_testbed(client_nodes, source_node):
        print(f"# -> Some nodes were down. Retrying ({retry}) times.")
        time.sleep(30)
        conf["retry"] -= 1
        run(conf)
        return

    if retry == 0:
        print("# -> Could not boot all nodes. Stopping.")
        return

    print("# -> Generating log file path")
    experiment_time = datetime.datetime.now().strftime("%Y-%m-%d_%H-%M-%S")
    logfile_path_base = f"/btree_data/{experiment_time}"

    print("# -> Preparing log folder on testbed controller")
    source = SSHClient(
        f"172.23.42.{source_node.id + 100}",
        user="root",
        password="raspberry",
        allow_agent=False,
    )
    source.run_command(f"mkdir {logfile_path_base}")

    print("# -> Preparing hosts for PSSH")
    pssh_nodes = [f"172.23.42.{node.id + 100}" for node in client_nodes]

    client_nodes = ParallelSSHClient(
        pssh_nodes, user="root", password="raspberry", allow_agent=False
    )

    print("# -> Starting client nodes")
    max_power = "--max_power" if conf['max_power'] else ""

    poll_timeout = f"--poll_timeout={conf['poll_timeout']}"
    discovery_bcast_interval = f"--broadcast_timeout={conf['discovery_bcast_interval']}"
    pending_timeout = f"--pending_timeout={conf['pending_timeout']}"
    source_retransmit_payload = f"--retransmit_timeout={conf['source_retransmit_payload']}"
    unchanged_counter = f"--unchanged_counter={conf['unchanged_counter']}"
    omit_roll_back = "--omit_roll_back" if conf['omit_roll_back'] else ""

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

    print("# -> Stopping potential zombie BTP processes")
    source.run_command("pkill -9 btp")
    client_nodes.run_command("pkill -9 btp")

    client_output = client_nodes.run_command(
        btp_cmd_common.format("", client_logfile_path)
    )
    for host_out in client_output:
        print(f"# -> {host_out.host} started")

    print("# -> Starting source node")
    source.run_command(f"dd bs={conf['payload_size']} count=1 </dev/urandom > source.file")

    print("# -> Starting experiment")
    source_logfile_path = f"{logfile_path_base}/source_$(hostname).log"

    source.run_command(
        btp_cmd_common.format("--source=source.file", source_logfile_path)
    )

    print("# -> Waiting for experiment to finish")
    time.sleep(conf['experiment_duration'])

    print("# -> Stopping BTP on all nodes")
    source.run_command("pkill --signal SIGINT btp")
    client_nodes.run_command("pkill --signal SIGINT btp")

    print("# -> Collecting logs and cleaning up")
    source.copy_remote_file(
        f"{logfile_path_base}",
        f"{os.getcwd()}/results/{experiment_time}",
        recurse=True,
    )

    with open(f"{os.getcwd()}/results/{experiment_time}/config", "w") as conf_file:
        toml.dump(conf, conf_file)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Run broadcast tree experiments on Piloty testbed"
    )

    parser.add_argument(
        "-c", "--config", help="Experiment configuration", required=True
    )

    args = parser.parse_args()
    conf = toml.load(args.config)

    node_filter = conf["CLIENTS"]
    source_id = conf["SOURCE"]["id"]
    experiment_config = conf["EXPERIMENTS"]

    experiment_duration = experiment_config["experiment_duration"]
    retry = experiment_config["retry"]

    for payload_size in experiment_config["payload_size"]:
        for max_power in experiment_config["max_power"]:
            for poll_timeout in experiment_config["poll_timeout"]:
                for discovery_bcast_interval in experiment_config["discovery_bcast_interval"]:
                    for pending_timeout in experiment_config["pending_timeout"]:
                        for source_retransmit_payload in experiment_config["source_retransmit_payload"]:
                            for unchanged_counter in experiment_config["unchanged_counter"]:
                                for omit_roll_back in experiment_config["omit_roll_back"]:
                                    for iteration in range(experiment_config["iterations"]):
                                        current_conf = {
                                            "payload_size": payload_size,
                                            "max_power": max_power,
                                            "poll_timeout": poll_timeout,
                                            "discovery_bcast_interval": discovery_bcast_interval,
                                            "pending_timeout": pending_timeout,
                                            "source_retransmit_payload": source_retransmit_payload,
                                            "unchanged_counter": unchanged_counter,
                                            "omit_roll_back": omit_roll_back,
                                            "experiment_duration": experiment_duration,
                                            "retry": retry,
                                            "iteration": iteration,
                                            "node_filter": node_filter,
                                            "source_id": source_id,
                                        }
                                        print(f"# Running iteration {iteration + 1} of configuration {current_conf}")
                                        run(
                                            current_conf
                                        )
