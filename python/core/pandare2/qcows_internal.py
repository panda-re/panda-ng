#!/usr/bin/env python3
'''
Image descriptions: the json files pandare2.mkimage writes (`Panda(generic=<json>)`).
'''

import json
from os import path
from collections import namedtuple

class Image(namedtuple('Image', ['arch', 'os', 'prompt', 'cdrom', 'snapshot', 'url', 'alternate_urls', 'extra_files', 'qcow', 'default_mem', 'extra_args', 'hashes'])):
    '''
    The Image class stores information about a PANDA image

    Args:
        arch (str): Arch for the given architecture.
        os (str): an os string we can pass to panda with -os
        prompt (regex): a regex to detect a bash prompt after loading the snapshot and sending commands
        cdrom (str): name to use for cd-drive when inserting an ISO via monitor
        qcow (str): the disk image, relative to the json
        default_mem (str): memory to use for the root snapshot (e.g. 1G)
        extra_args (list): Extra arguments to pass to PANDA (e.g. ['-display', 'none'])
        url, alternate_urls, extra_files, hashes: unused (classic PANDA's image downloads)
    '''

Image.__new__.__defaults__ = (None,) * len(Image._fields)

class Qcows():
    '''
    Reads image descriptions written by pandare2.mkimage.
    '''

    @staticmethod
    def load_image_json(json_path):
        '''
        Load an image description written by pandare2.mkimage. Its qcow is relative to the json.
        '''
        with open(json_path) as f:
            data = json.load(f)
        image = Image(**{k: data.get(k) for k in Image._fields})
        return image._replace(qcow=path.join(path.dirname(path.abspath(json_path)), image.qcow))

    @staticmethod
    def get_qcow_info(name):
        '''
        The Image for an image json written by pandare2.mkimage.

        Args:
            name (str): path to the image's .json

        Returns:
            Image: the image's description, with qcow as a path
        '''
        if name.endswith(".json") and path.isfile(name):
            return Qcows.load_image_json(name)
        raise ValueError(f"{name!r} is not an image json: build one with `python -m pandare2.mkimage` "
                         "(classic PANDA's generic images are gone: their snapshots don't load on panda-ng)")

    @staticmethod
    def get_qcow(name):
        '''
        The disk image of an image json written by pandare2.mkimage.

        Args:
            name (str): path to the image's .json

        Returns:
            string: Path to qcow
        '''
        return Qcows.get_qcow_info(name).qcow
