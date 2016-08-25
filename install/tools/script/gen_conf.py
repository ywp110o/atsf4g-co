#!/usr/bin/env python
# -*- coding: utf-8 -*-

import sys
import os, platform, locale
import shutil, re, string
from optparse import OptionParser
import glob
import common.print_color
import common.project_utils

console_encoding = sys.getfilesystemencoding()
script_dir = os.path.dirname(os.path.realpath(__file__))

if __name__ == '__main__':
    os.chdir(script_dir)

    parser = OptionParser("usage: %prog [options...]")
    parser.add_option("-e", "--env-prefix", action='store', dest="env_prefix", default='AUTOBUILD_', help="prefix when read parameters from environment variables")
    parser.add_option("-c", "--config", action='store', dest="config", default=os.path.join(script_dir, 'config.conf'), help="configure file(default: {0})".format(os.path.join(script_dir, 'config.conf')))
    parser.add_option("-s", "--set", action='append' , dest="set_vars", default=[], help="set configures")
    parser.add_option("-n", "--number", action='store' , dest="reset_number", default=None, type='int', help="set default server numbers")

    (opts, args) = parser.parse_args()
    # TODO reset all servers's number
    # TODO set all custom configures
    set_cmd_opts(opts)
    # TODO parse all services
    set_server_inst(None, 'atproxy', 1)
    # TODO use all template
    # http://www.makotemplates.org/