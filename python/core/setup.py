import setuptools
import os, re
from os.path import realpath, join, exists
from glob import glob
import distutils.command.build

root = realpath("../..")

class build_python_autogen(setuptools.Command):
    def initialize_options(self):
        pass
    def finalize_options(self):
        pass
    def run(self):
        import pandare_build
        local_panda_ng = realpath(join(root, "local_packages/panda-ng"))
        if exists(local_panda_ng):
            panda_ng_path = local_panda_ng
        elif exists("/usr/include/panda-ng"):
            panda_ng_path = "/usr/include/panda-ng"
        else:
            raise Exception("Could not find panda-ng path. Considered /usr/include/panda-ng and local_packages/panda-ng")
        paths = glob(join(panda_ng_path, "panda_python_*.h"))
        plugin_dir = realpath(join(root, "plugins"))
        if not exists(plugin_dir):
            raise Exception("Could not find plugin directory. This should be impossible. Build environment issue. {}".format(repr(plugin_dir)))
        for path in paths:
            arch = re.search("panda_python_(.*).h", path).group(1)
            print(f"Building auto-generated python bindings for {arch}")
            pandare_build.handle_python(arch, open(path).read(), plugin_dir)

class build(distutils.command.build.build):
    _sub_command = ('build_python_autogen', None, )
    _sub_commands = distutils.command.build.build.sub_commands
    sub_commands = [_sub_command] + _sub_commands

if os.path.exists("pandare2/version.txt"):
    with open("pandare2/version.txt") as f:
        version = f.read().strip()
else:
    version = "0.0.1"
setuptools.setup(version=version, cmdclass=dict(build_python_autogen=build_python_autogen, build=build))
