#!/usr/bin/env python3
'''
Image descriptions: the files pandare2.mkimage writes (`Panda(generic=<json>)`), as JSON or YAML
with the same keys, or the same keys in a dict.
'''

import json
from os import path
from collections import namedtuple

class Image(namedtuple('Image', ['arch', 'os', 'prompt', 'cdrom', 'snapshot', 'url', 'alternate_urls', 'extra_files', 'qcow', 'default_mem', 'extra_args', 'hashes', 'isf', 'kconf_file'])):
    '''
    The Image class stores information about a PANDA image

    Args:
        arch (str): Arch for the given architecture.
        os (str): an os string we can pass to panda with -os
        prompt (regex): a regex to detect a bash prompt after loading the snapshot and sending commands
        cdrom (str): name to use for cd-drive when inserting an ISO via monitor
        qcow (str): the disk image, relative to the image file
        default_mem (str): memory to use for the root snapshot (e.g. 1G)
        extra_args (str or list): Extra arguments to pass to PANDA (e.g. "-display none")
        isf (str): the guest kernel's Volatility 3 ISF (osi_isf's isf), relative to the image file
        kconf_file (str): the guest kernel's osi_linux kernelinfo file, relative to the image file
        url, alternate_urls, extra_files, hashes: unused (classic PANDA's image downloads)
    '''

Image.__new__.__defaults__ = (None,) * len(Image._fields)

# The keys that name files: relative to the image file (to the working directory for a dict).
_PATHS = ('qcow', 'isf', 'kconf_file')

class Qcows():
    '''
    Reads image descriptions written by pandare2.mkimage.
    '''

    @staticmethod
    def load_image_json(json_path):
        '''
        Load an image description written by pandare2.mkimage. Its qcow is relative to the json.
        '''
        return Qcows.get_qcow_info(json_path)

    @staticmethod
    def _image(data, base, where):
        if not isinstance(data, dict):
            raise ValueError(f"{where}: an image description is a mapping of its keys, not {type(data).__name__}")
        if not data.get('arch'):
            raise ValueError(f"{where}: the image has no 'arch'")
        image = Image(**{k: data.get(k) for k in Image._fields})
        return image._replace(**{k: path.join(base, path.expanduser(getattr(image, k)))
                                 for k in _PATHS if getattr(image, k)})

    @staticmethod
    def get_qcow_info(name):
        '''
        The Image for an image description written by pandare2.mkimage.

        Args:
            name (str or dict): path to the image's .json, .yaml or .yml (the format is chosen by
                the extension; the keys are the same), or a dict of those keys. The files it
                names (qcow, isf, kconf_file) are relative to the image file, or for a dict to
                the working directory.

        Returns:
            Image: the image's description, with its files as paths
        '''
        if isinstance(name, dict):
            return Qcows._image(name, path.abspath("."), "generic=<dict>")
        name = path.expanduser(str(name))
        ext = path.splitext(name)[1].lower()
        if ext not in (".json", ".yaml", ".yml"):
            raise ValueError(f"{name!r} is not an image description (.json, .yaml or .yml): build one with "
                             "`python -m pandare2.mkimage` (classic PANDA's generic images are gone: their "
                             "snapshots don't load on panda-ng)")
        if not path.isfile(name):
            raise FileNotFoundError(f"image description {name!r} doesn't exist")
        with open(name) as f:
            if ext == ".json":
                data = json.load(f)
            else:
                import yaml
                data = yaml.safe_load(f)
        return Qcows._image(data, path.dirname(path.abspath(name)), name)

    @staticmethod
    def get_qcow(name):
        '''
        The disk image of an image description written by pandare2.mkimage.

        Args:
            name (str or dict): see `get_qcow_info`

        Returns:
            string: Path to qcow
        '''
        return Qcows.get_qcow_info(name).qcow
