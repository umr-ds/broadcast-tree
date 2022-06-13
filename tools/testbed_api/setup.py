#!/usr/bin/env python3

import setuptools

setuptools.setup(
    name="testbed-api",
    version="0.1",
    description="Functions to interact with the Piloty testbed",
    packages=setuptools.find_packages(),
    install_requires=["requests"],
    scripts=["testbed_api/testbed_client.py"],
)
