import argparse

import testbed_api.client as client

import tpycontrol as tpy


def boot_nodes(args):
    node_filter = {"switch": "A0"}

    conf = open(args.config, "w")

    conf.write(
        """
[DEFAULT]
port = 42337
tmpdir = ~/
    """
    )

    nodes = client.get_nodes_by_filter(**node_filter)

    for node in nodes:
        suffix = 100 + node.id
        ip_addr = f"172.23.42.{suffix}"

        conf.write(
            f"""
[{node.id}]
host = {ip_addr}
"""
        )

    client.graceful_boot_nodes(nodes)


def run(args):
    # read the device configuration
    devices = tpy.Devices(args.config)

    # connect to the devices and initialize the controller
    tc = tpy.TPyControl(devices)

    print(tc)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Run broadcast tree experiments on Piloty testbed"
    )

    parser.add_argument(
        "-s",
        "--source",
        type=int,
        help="Node ID of broadcast tree source node",
        required=True,
    )
    parser.add_argument(
        "-c", "--config", help="TPy config of nodes to use as clients", required=True
    )

    args = parser.parse_args()

    boot_nodes(args)

    # run(args)
