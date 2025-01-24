from setuptools import setup
import os

if os.path.exists("pandare2/version.txt"):
    with open("pandare2/version.txt") as f:
        version = f.read().strip()
else:
    version = "0.0.1"
setup(version=version)