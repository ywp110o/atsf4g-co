#!/usr/bin/env python
# -*- coding: utf-8 -*-

import sys
import os, ctypes, platform
import cgi


cmd_opts = None
server_opts = None
server_name = ''
server_index = 1
server_cache_id = None
server_cache_full_name = None

def set_cmd_opts(opts):
    cmd_opts = opts

def set_server_inst(opts, key, index):
    server_opts = opts
    server_name = key
    server_index = index
    server_cache_id = None
    server_cache_full_name = None

def get_inner_ipv4():
    if 'SYSTEM_MACRO_INNER_IPV4' in os.environ:
        return os.environ['SYSTEM_MACRO_INNER_IPV4']
    # detect inner ip address
    return '127.0.0.1'


def get_outer_ipv4():
    if 'SYSTEM_MACRO_OUTER_IPV4' in os.environ:
        return os.environ['SYSTEM_MACRO_OUTER_IPV4']
    # detect inner ip address
    return '0.0.0.0'

def get_inner_ipv6():
    if 'SYSTEM_MACRO_INNER_IPV6' in os.environ:
        return os.environ['SYSTEM_MACRO_INNER_IPV6']
    # detect inner ip address
    return '::1'


def get_outer_ipv6():
    if 'SYSTEM_MACRO_OUTER_IPV6' in os.environ:
        return os.environ['SYSTEM_MACRO_OUTER_IPV6']
    # detect inner ip address
    return '::'

def get_hostname():
    if 'SYSTEM_MACRO_HOST_NAME' in os.environ:
        return os.environ['SYSTEM_MACRO_HOST_NAME']
    return ''

def get_global_option(section, key, default_val, env_name = None):
    if not env_name is None and env_name in os.environ:
        return os.environ[env_name]
    
    if cmd_opts.has_option(section, key):
        return cmd_opts.get(section, key)
    
    return default_val

def get_server_name():
    return server_name

def get_server_option(key, default_val, env_name = None):
    return get_global_option('server.{0}'.format(get_server_name()), key, default_val, env_name)

def get_server_index():
    return server_index

def get_server_id():
    if not server_cache_id is None:
        return server_cache_id

    if not cmd_opts.has_option('atservice', get_server_name()):
        return 0
    group_id = get_global_option('global', 'group_id', 1)
    group_step = get_global_option('global', 'group_step', 0x10000)
    type_step = get_global_option('global', 'type_step', 0x100)
    type_id = get_global_option('atservice', get_server_name(), 0)
    server_cache_id = group_id * group_step + type_step * type_id + get_server_index()
    return server_cache_id

def get_server_full_name():
    if not server_cache_full_name is None:
        return server_cache_full_name
    server_cache_full_name = '{0}-{1}'.format(get_server_name(), get_server_index())
    return server_cache_full_name
