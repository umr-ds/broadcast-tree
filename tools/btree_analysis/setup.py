#!/usr/bin/env python3

import setuptools

setuptools.setup(
    name="btree_analysis",
    version="0.1",
    description="Analyse btree experiments",
    packages=setuptools.find_packages(),
    install_requires=["graphviz"]
)
