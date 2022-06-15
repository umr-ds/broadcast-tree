#! /usr/bin/env python3

##################################################################
##################################################################
# TODO: THIS NEEDS TO BE FIXED AS SOON AS TESTBED API IS STABLED #
##################################################################
##################################################################

import logging

from dataclasses import dataclass

import requests

BASE_URL = "http://172.23.42.253"
APPLICATION_HEADER = {"accept": "application/json"}


@dataclass
class Node:
    id: int
    floor: str
    room: str
    mac: str
    patchpanel: str
    switch: str
    switchport: str
    note: str
    power: int


# Functions to get nodes
def get_nodes() -> list[Node]:
    logging.debug("Requesting all nodes")
    endpoint_url = f"{BASE_URL}/nodes"
    r = requests.get(endpoint_url, headers=APPLICATION_HEADER)

    nodes_json = r.json()
    nodes = [Node(**json_data) for json_data in nodes_json]

    return nodes


def get_node(node_id: int) -> Node:
    logging.debug(f"Requesting node {node_id}")
    endpoint_url = f"{BASE_URL}/nodes/{node_id}"
    r = requests.get(endpoint_url, headers=APPLICATION_HEADER)

    return Node(**r.json())


# Functions for enabling and disabling ports
def enable_port(node_id: int) -> None:
    logging.debug(f"Enabling port for node {node_id}")
    endpoint_url = f"{BASE_URL}/nodes/enable/{node_id}"
    requests.get(endpoint_url, headers=APPLICATION_HEADER)


def disable_port(node_id: int) -> None:
    logging.debug(f"Disabling port for node {node_id}")
    endpoint_url = f"{BASE_URL}/nodes/disable/{node_id}"
    requests.get(endpoint_url, headers=APPLICATION_HEADER)


# Functions for booting and turning off nodes
def boot_nodes() -> None:
    logging.debug(f"Enabling all ports")
    endpoint_url = f"{BASE_URL}/nodes/on"
    requests.get(endpoint_url, headers=APPLICATION_HEADER)


def shutdown_nodes() -> None:
    logging.debug(f"Disabling all ports")
    endpoint_url = f"{BASE_URL}/nodes/off"
    requests.get(endpoint_url, headers=APPLICATION_HEADER)


def reboot_nodes() -> None:
    logging.debug(f"Rebooting all nodes (expect missing nodes!)")
    endpoint_url = f"{BASE_URL}/nodes/reset"
    requests.get(endpoint_url, headers=APPLICATION_HEADER)


def boot_node(node_id: int) -> None:
    logging.debug(f"Booting node {node_id}")
    endpoint_url = f"{BASE_URL}/nodes/on/{node_id}"
    requests.get(endpoint_url, headers=APPLICATION_HEADER)


def shutdown_node(node_id: int) -> None:
    logging.debug(f"Shutting down node {node_id}")
    endpoint_url = f"{BASE_URL}/nodes/off/{node_id}"
    requests.get(endpoint_url, headers=APPLICATION_HEADER)


def reboot_node(node_id: int) -> None:
    logging.debug(f"Rebooting node {node_id}")
    endpoint_url = f"{BASE_URL}/nodes/reset/{node_id}"
    requests.get(endpoint_url, headers=APPLICATION_HEADER)
