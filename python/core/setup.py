from setuptools import setup
import os, re
from os.path import realpath, join, exists
from pandare_build import handle_python
from glob import glob

root = realpath("../..")

def run_install():
    local_panda_ng = realpath(join(root, "local_packages/panda-ng"))
    if exists(local_panda_ng):
        panda_ng_path = local_panda_ng
    elif exists("/usr/include/panda-ng"):
        panda_ng_path = "/usr/include/panda-ng"
    else:
        raise Exception("Could not find panda-ng path. Considered /usr/include/panda-ng and local_packages/panda-ng")
    paths = glob(join(panda_ng_path, "panda_python_*.h"))
    for path in paths:
        arch = re.search("panda_python_(.*).h", path).group(1)
        print(f"Building auto-generated python bindings for {arch}")
        handle_python(arch, open(path).read())

run_install()
if os.path.exists("pandare2/version.txt"):
    with open("pandare2/version.txt") as f:
        version = f.read().strip()
else:
    version = "0.0.1"
setup(version=version)