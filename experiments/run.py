import argparse
import time
import datetime
import os
import shutil

import toml

from pssh.clients import ParallelSSHClient, SSHClient

import testbed_api.client as client
import testbed_api.api as api


def run(node_filter, source_id, experiment_config, iteration, local):
    print("Running prepare stage")

    _nodes = client.get_nodes_by_filter(**node_filter)
    client_nodes = [node for node in _nodes if node.id != source_id]
    source_node = client.get_nodes_by_filter(**{"id": [source_id]})[0]

    print("-> Generating log file path")
    experiment_time = datetime.datetime.now().strftime("%Y-%m-%d_%H-%M-%S")
    logfile_path_base = f"/btree_data/{experiment_time}"

    print("Running run stage")

    print("Preparing log folder on testbed contoller")
    if local:
        os.mkdir(f"/srv/nfs{logfile_path_base}")
    else:
        controller = SSHClient(
            "172.23.42.1", user="testbed", password="testbed", allow_agent=False
        )
        controller.run_command(f"mkdir /srv/nfs{logfile_path_base}")

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
        f'bash -c "nohup btp --log_level=2 {max_power} {iface} > {logfile_path} 2>&1 &"'
    )
    client_output = client_nodes.run_command(btp_client_cmd)
    for host_out in client_output:
        print(f"{host_out.host} started")

    print("-> Starting source node")
    source = SSHClient(
        f"172.23.42.{source_node.id + 100}",
        user="root",
        password="raspberry",
        allow_agent=False,
    )

    source.run_command(
        f"dd bs={experiment_config['payload_size']} count=1 </dev/urandom > source.file"
    )

    print("-> Starting experiment")
    max_power = "--max_power" if experiment_config["max_power"] else ""
    iface = "$(grep -l b8:27 /sys/class/net/wlan*/address | cut -d'/' -f5)"
    logfile_path = f"{logfile_path_base}/source_$(hostname).log"

    btp_client_cmd = f'bash -c "nohup btp --source=source.file --log_level=2 {max_power} {iface} > {logfile_path} 2>&1 &"'
    source.run_command(btp_client_cmd)

    print("-> Waiting for experiment to finish")
    time.sleep(10)

    print("-> Stopping BTP on all nodes")
    source.run_command("pkill btp")
    client_nodes.run_command("pkill btp")

    print("-> Collecting logs and cleaning up")
    if local:
        shutil.copytree(f"/srv/nfs{logfile_path_base}", f"~/{experiment_time}")
    else:
        controller.copy_remote_file(
            f"/srv/nfs{logfile_path_base}",
            f"{os.getcwd()}/results/{experiment_time}",
            recurse=True,
        )
        controller.run_command(f"rm -rf /srv/nfs{logfile_path_base}")

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
    parser.add_argument(
        "-l",
        "--local",
        action="store_true",
        help="Whether this script is executed directly on the testbed controller or not",
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
            args.local,
        )
