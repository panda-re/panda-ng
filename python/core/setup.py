from setuptools import setup
import os, re
from os.path import realpath, join, exists
import setuptools.command.build_py
from pandare_build import handle_python
from glob import glob

root = realpath("../..")

class InstallCommand(setuptools.command.build_py.build_py):
  """Custom install command."""

  def run(self):
    local_panda_ng = realpath(join(root, "local_packages/panda-ng"))
    if exists(local_panda_ng):
        panda_ng_path = local_panda_ng
    elif exists("/usr/include/panda-ng"):
        panda_ng_path = "/usr/include/panda-ng"
    else:
        raise Exception("Could not find panda-ng path. Considered /usr/include/panda-ng and local_packages/panda-ng")
    paths = glob(join(panda_ng_path, "panda_python_*.h"))
    for path in paths:
        handle_python(re.search("panda_python_(.*).h", path).group(1), open(path).read())

if os.path.exists("pandare2/version.txt"):
    with open("pandare2/version.txt") as f:
        version = f.read().strip()
else:
    version = "0.0.1"
setup(version=version,
    cmdclass={
        'install': InstallCommand,
        'develop': InstallCommand,
    })