"""pytest's entry to the panda-ng tests: the collector and runner are tests/pandatest (tests/run)."""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
pytest_plugins = ["pandatest.plugin"]
