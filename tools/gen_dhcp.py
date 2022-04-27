#! /usr/bin/env python3
import logging

import testbed_api.api as api

nodes = api.get_nodes()

for node in nodes:
    mac_addr_parts = node.mac.split(":")
    fst = mac_addr_parts[-3]
    snd = mac_addr_parts[-2]
    trd = mac_addr_parts[-1]
    hname = f"btp-{fst}-{snd}-{trd}"

    suffix = 100 + node.id

    print(
        f"""host {hname} {{
    hardware ethernet {node.mac};
    fixed-address 172.23.42.{suffix};
}}"""
    )
