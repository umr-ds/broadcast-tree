import logging
import subprocess
import os

import fcntl
import socket
import struct

import Pyro4

import netifaces

from tpynode import TPyModule

logger = logging.getLogger(__name__)
logger.setLevel(logging.DEBUG)


def _gen_payload(path, size):
    with open(path, "wb") as f:
        f.write(os.urandom(size))


def get_interface():
    interfaces = [
        interface for interface in netifaces.interfaces() if "wlan" in interface
    ]
    for interface in interfaces:
        if netifaces.ifaddresses(i)[netifaces.AF_LINK][0]["addr"].startswith("b8:27"):
            return interface


def get_ip(interface):
    return netifaces.ifaddresses(interface)[AF_INET][0]["addr"]


class BTree(TPyModule):
    def __init__(self, **kwargs):
        super(BTree, self).__init__(**kwargs)
        self._mode = kwargs.get("mode", "client")
        self._size = kwargs.get("size", 1024)
        self._max_power = kwargs.get("max_power", None)
        self._experiment_id = kwargs.get("experiment_id", None)

    @Pyro4.expose
    def btp(self):
        interface = get_interface()

        cmd = ["btp", "--log_level", "2"]

        node_id = int(get_ip(interface).split(".")[-1]) - 100
        logfile_path = f"/btree_data/{self._experiment_id}/{node_id}.log"
        cmd.append(f"--log_file={logfile_path}")

        if self._mode == "source":
            payload_path = "/tmp/payload"
            _gen_payload(payload_path, self._size)
            cmd.append(f"--source={payload_path}")

        if self._max_power:
            cmd.append(f"--max_power")

        cmd.append(interface)

        return subprocess.run(cmd).returncode == 0
