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


def run(conf, repeat):
    if not repeat:
        if experiment_already_done(conf):
            print("# -> Experiment already done.")
            return

    _nodes = client.get_nodes_by_filter(**node_filter)
    client_nodes = [node for node in _nodes if node.id != source_id]
    source_node = client.get_nodes_by_filter(**{"id": [source_id]})[0]

    if not fix_testbed(client_nodes, source_node):
        print(f"# -> Some nodes were down. Retrying {conf['retry']} times.")
        time.sleep(30)
        conf["retry"] -= 1

        if conf["retry"] == 0:
            print("# -> Could not boot all nodes. Stopping.")
            return
        else:
            run(conf, repeat)
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

    print("# -> Preparing clients for PSSH")
    pssh_nodes = [f"172.23.42.{node.id + 100}" for node in client_nodes]

    client_nodes = ParallelSSHClient(
        pssh_nodes, user="root", password="raspberry", allow_agent=False
    )

    print("# -> Stopping potential zombie BTP processes")
    source.run_command("pkill -9 btp")
    client_nodes.run_command("pkill -9 btp")

    print("# -> Starting client nodes")
    flood = "--flood" if conf['flood'] else ""
    poll_timeout = f"--poll_timeout={conf['poll_timeout']}"
    discovery_bcast_interval = f"--broadcast_timeout={conf['discovery_bcast_interval']}"
    pending_timeout = f"--pending_timeout={conf['pending_timeout']}"
    source_retransmit_payload = f"--retransmit_timeout={conf['source_retransmit_payload']}"
    unchanged_counter = f"--unchanged_counter={conf['unchanged_counter']}"
    tx_pwr_threshold = f"--tx_pwr_threshold={conf['tx_pwr_threshold']}"
    omit_roll_back = "--omit_roll_back" if conf['omit_roll_back'] else ""
    iface = "$(grep -l b8:27 /sys/class/net/wlan*/address | cut -d'/' -f5)"
    client_logfile_path = f"{logfile_path_base}/$(hostname).log"
    source_logfile_path = f"{logfile_path_base}/source_$(hostname).log"

    btp_cmd_common = (
        f'bash -c -i -l "nohup btp --log_level=2'
        f" {{0}}"
        f" {flood} {omit_roll_back} {unchanged_counter}"
        f" {poll_timeout} {discovery_bcast_interval} {pending_timeout} {source_retransmit_payload} {tx_pwr_threshold}"
        f' --log_file={{1}} {iface} &"'
    )

    client_output = client_nodes.run_command(
        btp_cmd_common.format("", client_logfile_path)
    )
    for host_out in client_output:
        print(f"# -> {host_out.host} started")

    print("# -> Starting source node")
    source.run_command(f"dd bs={conf['payload_size']} count=1 </dev/urandom > source.file")

    print("# -> Starting experiment")
    time.sleep(20)

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

    source.run_command(f"rm -rf {logfile_path_base}")
    client_nodes.run_command("rm /root/btp*")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Run broadcast tree experiments on Piloty testbed"
    )

    parser.add_argument(
        "-c", "--config", help="Experiment configuration", required=True
    )
    parser.add_argument(
        "-r", "--repeat", help="Repeat already done experiments", action="store_true"
    )

    args = parser.parse_args()
    conf = toml.load(args.config)

    node_filter = conf["CLIENTS"]
    experiment_config = conf["EXPERIMENTS"]

    experiment_duration = experiment_config["experiment_duration"]
    retry = experiment_config["retry"]

    for payload_size in experiment_config["payload_size"]:
        for flood in experiment_config["flood"]:
            for poll_timeout in experiment_config["poll_timeout"]:
                for discovery_bcast_interval in experiment_config["discovery_bcast_interval"]:
                    for pending_timeout in experiment_config["pending_timeout"]:
                        for source_retransmit_payload in experiment_config["source_retransmit_payload"]:
                            for unchanged_counter in experiment_config["unchanged_counter"]:
                                for omit_roll_back in experiment_config["omit_roll_back"]:
                                    for iteration in range(experiment_config["iterations"]):
                                        for tx_pwr_threshold in experiment_config["tx_pwr_threshold"]:
                                            for source_id in conf["SOURCE"]["id"]:
                                                current_conf = {
                                                    "payload_size": payload_size,
                                                    "flood": flood,
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
                                                    "tx_pwr_threshold": tx_pwr_threshold,
                                                }
                                                print(f"# Running iteration {iteration + 1} of configuration {current_conf}")
                                                run(
                                                    current_conf,
                                                    args.repeat
                                                )
