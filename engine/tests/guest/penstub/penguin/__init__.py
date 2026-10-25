"""Stand-ins for the parts of penguin's runtime that pyplugins/apis/mem.py and
pyplugins/hyper/portal.py import, so pyhost.py can run them unchanged."""
import logging


class _Plugins:
    pass


plugins = _Plugins()
plugins.kffi = None             # hyper/portal.py reads it at import


class PluginArgs:
    """penguin's plugin argument schema base (pydantic there; declarations only here:
    Plugin.args is the plain dict penhost.py sets)"""


class Plugin:
    panda = None                # set by pyhost.py / penhost.py
    args = {}                   # every plugin's arguments (penhost.py)

    @property
    def logger(self):
        return logging.getLogger(type(self).__name__)

    def get_arg(self, name):
        return self.args.get(name)


def getColoredLogger(name):
    return logging.getLogger(name)
