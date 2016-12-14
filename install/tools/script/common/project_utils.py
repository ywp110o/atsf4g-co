#!/usr/bin/env python
# -*- coding: utf-8 -*-

import sys
import os, ctypes, platform
import cgi, re

environment_check_shm = None
global_opts = None
global_cahce = dict(id_offset=0,services_type=[])
server_opts = None
server_name = ''
server_index = 1
server_alloc_listen_port = 0
server_proxy_addr = ''
server_cache_id = None
server_cache_full_name = None
server_cache_ip = dict()

def set_global_opts(opts, id_offset):
    global global_opts
    global global_cahce
    global_opts = opts
    global_cahce['id_offset'] = id_offset
    for server_type in sorted(opts.items('atservice'), key=lambda x:int(x[1])):
        if 'atgateway' == server_type[0]:
            continue
        svr_name = 'server.{0}'.format(server_type[0])
        if global_opts.has_section(svr_name):
            global_cahce['services_type'].append(server_type[0])

def get_service_index_range(number = 1):
    return range(1 + global_cahce['id_offset'], 1 + global_cahce['id_offset'] + number)

def get_global_all_services():
    return global_cahce['services_type']

def set_server_inst(opts, key, index):
    global server_opts
    global server_name
    global server_index
    global server_cache_id
    global server_cache_full_name
    server_opts = opts
    server_name = key
    server_index = index
    server_cache_id = None
    server_cache_full_name = None

def get_ip_list_v4():
    global server_cache_ip
    if 'ipv4' not in server_cache_ip:
        import socket
        server_cache_ip['ipv4'] = []
        try:
            for ip_pair in socket.getaddrinfo(socket.gethostname(), 0, socket.AF_INET, socket.SOCK_STREAM):
                ip_addr = ip_pair[4][0]
                if '127.0.0.1' != ip_addr:
                    server_cache_ip['ipv4'].append(ip_addr)
            # use socket to detect ipv6 address if can not find any address
            if 0 == len(server_cache_ip['ipv4']):
                csock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
                csock.connect(('8.8.8.8', 53)) # use google's DNS
                res = csock.getsockname()
                if res:
                    server_cache_ip['ipv4'].append(res[0])
                csock.close()
        except:
            pass
    return server_cache_ip['ipv4']

def get_ip_list_v6():
    global server_cache_ip
    if 'ipv6' not in server_cache_ip:
        import socket
        server_cache_ip['ipv6'] = []
        try:
            for ip_pair in socket.getaddrinfo(socket.gethostname(), 0, socket.AF_INET6, socket.SOCK_STREAM):
                ip_addr = ip_pair[4][0]
                interface_index = ip_addr.find('%')
                # remove interface name
                if interface_index > 0:
                    ip_addr = ip_addr[0:interface_index]
                if not ipaddress.ip_address(ip_addr).is_private:
                    server_cache_ip['ipv6'].append(ip_addr)
            # use socket to detect ipv6 address if can not find any address
            if 0 == len(server_cache_ip['ipv6']):
                csock = socket.socket(socket.AF_INET6, socket.SOCK_DGRAM)
                csock.connect(('2001:4860:4860::8888', 53)) # use google's DNS
                res = csock.getsockname()
                if res:
                    server_cache_ip['ipv6'].append(res[0])
                csock.close()
        except:
            pass
    return server_cache_ip['ipv6']

def is_ip_v6_enabled():
    ipv6s = get_ip_list_v6()
    return len(ipv6s) > 0

def get_inner_ipv4():
    if 'SYSTEM_MACRO_INNER_IPV4' in os.environ:
        return os.environ['SYSTEM_MACRO_INNER_IPV4']
    # detect inner ip address
    res = get_ip_list_v4()
    if 0 == len(res):
        return '127.0.0.1'
    return res[0]


def get_outer_ipv4():
    if 'SYSTEM_MACRO_OUTER_IPV4' in os.environ:
        return os.environ['SYSTEM_MACRO_OUTER_IPV4']
    # detect inner ip address
    res = get_ip_list_v4()
    if 0 == len(res):
        return '0.0.0.0'
    ret = res[0]
    if '127.0.0.1' == ret:
        ret = '0.0.0.0'
    return ret

def get_inner_ipv6():
    if 'SYSTEM_MACRO_INNER_IPV6' in os.environ:
        return os.environ['SYSTEM_MACRO_INNER_IPV6']
    # detect inner ip address
    res = get_ip_list_v6()
    if 0 == len(res):
        return '::1'
    return res[0]

def get_outer_ipv6():
    if 'SYSTEM_MACRO_OUTER_IPV6' in os.environ:
        return os.environ['SYSTEM_MACRO_OUTER_IPV6']
    # detect inner ip address
    res = get_ip_list_v6()
    if 0 == len(res):
        return '::'
    ret = res[0]
    if '::1' == ret:
        ret = '::'
    return ret

def get_hostname():
    if 'SYSTEM_MACRO_HOST_NAME' in os.environ:
        return os.environ['SYSTEM_MACRO_HOST_NAME']
    return ''

def get_global_option(section, key, default_val, env_name = None):
    global global_opts
    if not env_name is None and env_name in os.environ:
        return os.environ[env_name]
    
    if global_opts.has_option(section, key):
        return global_opts.get(section, key)
    
    return default_val

def str_to_list(val):
    ret = []
    if val is None:
        return ret
    for item in str(val).split(','):
        item_strip = item.strip()
        if len(item_strip) > 0:
            ret.append(item_strip)
    return ret

def list_to_hosts(val):
    ret = []
    mat = re.compile('([^:]*):(\d+)-(\d+)\s*$')
    for item in val:
        mat_res = mat.match(item)
        if not mat_res is None:
            for i in range(int(mat_res.group(2)), int(mat_res.group(3)) + 1):
                ret.append('{0}:{1}'.format(mat_res.group(1), i))
        else:
            ret.append(item)
    return ret

def str_to_hosts(val):
    return list_to_hosts(str_to_list(val))

def get_global_list(section, key, default_val, env_name = None):
    res = get_global_option(section, key, default_val, env_name)
    if res is None:
        return []
    return str_to_list(res)

def get_global_list_to_hosts(section, key, default_val, env_name = None):
    res = get_global_list(section, key, default_val, env_name)
    return list_to_hosts(res)

def get_global_option_bool(section, key, default_val, env_name = None):
    val = get_global_option(section, key, default_val, env_name)
    if not val:
        return False
    val = str(val).lower().strip()
    return len(val) > 0 and '0' != val and 'false' != val and 'no' != val and 'disable' != val

def get_server_name():
    global server_name
    return server_name

def get_server_option(key, default_val, env_name = None):
    return get_global_option('server.{0}'.format(get_server_name()), key, default_val, env_name)

def get_server_list(key, default_val, env_name = None):
    return get_global_list('server.{0}'.format(get_server_name()), key, default_val, env_name)

def get_server_list_to_hosts(key, default_val, env_name = None):
    return get_global_list_to_hosts('server.{0}'.format(get_server_name()), key, default_val, env_name)

def get_server_index():
    global server_index
    return server_index

def get_server_group_inner_id(server_name = None, server_index = None):
    global global_opts
    if server_name is None:
        server_name = get_server_name()
    if server_index is None:
        server_index = get_server_index()

    if not global_opts.has_option('atservice', server_name):
        return 0
    type_step = int(get_global_option('global', 'type_step', 0x100))
    type_id = int(get_global_option('atservice', server_name, 0))
    return type_step * type_id + server_index

def get_server_proc_id(server_name = None, server_index = None):
    group_id = int(get_global_option('global', 'group_id', 1, 'SYSTEM_MACRO_GROUP_ID'))
    group_step = int(get_global_option('global', 'group_step', 0x10000, 'SYSTEM_MACRO_GROUP_STEP'))
    return group_id * group_step + get_server_group_inner_id(server_name, server_index)

def get_server_id():
    global server_cache_id
    global global_opts
    if not server_cache_id is None:
        return server_cache_id

    if not global_opts.has_option('atservice', get_server_name()):
        return 0
    server_cache_id = get_server_proc_id()
    return server_cache_id

def get_server_full_name():
    global server_cache_full_name
    if not server_cache_full_name is None:
        return server_cache_full_name
    server_cache_full_name = '{0}-{1}'.format(get_server_name(), get_server_index())
    return server_cache_full_name


def get_log_level():
    return get_global_option('global', 'log_level', 6, 'SYSTEM_MACRO_CUSTOM_LOG_LEVEL')

def get_log_dir():
    return get_global_option('global', 'log_dir', '../log', 'SYSTEM_MACRO_CUSTOM_LOG_DIR')

def get_server_atbus_shm():
    global environment_check_shm
    if environment_check_shm is None:
        # check if it support shm
        if not os.path.exists('/proc/sys/kernel/shmmax'):
            environment_check_shm = False
        else:
            shm_max_sz = int(open('/proc/sys/kernel/shmmax', 'r').read())
            environment_check_shm = shm_max_sz > 0
    
    if not environment_check_shm:
        return None
    base_key = int(get_global_option('atsystem', 'shm_key_pool', 0x16000000, 'SYSTEM_MACRO_CUSTOM_SHM_KEY'))
    shm_key = base_key + get_server_group_inner_id(get_server_name(), get_server_index())
    return 'shm://{0}'.format(hex(shm_key))

def get_calc_listen_port(server_name = None, server_index = None, base_port = 'port'):
    if server_name is None:
        server_name = get_server_name()
    if server_index is None:
        server_index = get_server_index()
    
    ret = int(get_global_option('server.{0}'.format(server_name), base_port, 0))
    if 0 == ret:
        base_port = int(get_global_option('atsystem', 'listen_port', 23000, 'SYSTEM_MACRO_CUSTOM_BASE_PORT'))
        type_step = int(get_global_option('global', 'type_step', 0x100))
        type_id = int(get_global_option('atservice', server_name, 0))
        return base_port + type_step * server_index + type_id
    else:
        return ret + server_index

def get_server_atbus_port():
    return get_calc_listen_port()

def get_server_atbus_tcp():
    if is_ip_v6_enabled():
        if 'atproxy' == get_server_name():
            return 'ipv6://{0}:{1}'.format(get_outer_ipv6(), get_server_atbus_port())
        else:
            return 'ipv6://{0}:{1}'.format(get_inner_ipv6(), get_server_atbus_port())
    else:
        if 'atproxy' == get_server_name():
            return 'ipv4://{0}:{1}'.format(get_outer_ipv4(), get_server_atbus_port())
        else:
            return 'ipv4://{0}:{1}'.format(get_inner_ipv4(), get_server_atbus_port())

def get_server_atbus_listen():
    ret = []
    res = get_server_atbus_shm()
    if not res is None:
        ret.append(res)
    if 0 == len(ret) or 'atproxy' == get_server_name():
        ret.append(get_server_atbus_tcp())
    return ret

def get_server_proxy():
    global server_proxy_addr
    if 'atproxy' == get_server_name():
        server_proxy_addr = get_server_atbus_tcp()
        return ''
    return server_proxy_addr
    
def get_server_children_mask():
    return get_server_option('children_mask', 0)

def get_server_recv_buffer_size():
    return get_global_option('atsystem', 'shm_key_pool', 2 * 1024 * 1024)

def get_server_gateway_index(server_name = None, server_index = None):
    if server_name is None:
        server_name = get_server_name()
    if server_index is None:
        server_index = get_server_index()

    step = int(get_global_option('server.atgateway', 'index_type_number', 1))
    offset = get_global_option('server.atgateway', 'index_map_{0}'.format(server_name), None)
    if offset is None:
        raise Exception('index_map_{0} is not found in server.atgateway'.format(server_name))
    return step * server_index + int(offset)

def get_server_gateway_port(server_name = None, server_index = None, base_port = 'atgateway_port'):
    if server_name is None:
        server_name = get_server_name()
    if server_index is None:
        server_index = get_server_index()
    ret = int(get_global_option('server.{0}'.format(server_name), base_port, 0))
    if 0 != ret:
        return ret + server_index
    ret = int(get_global_option('server.atgateway', 'default_port', 8000))
    return ret + get_server_gateway_index(server_name, server_index)
