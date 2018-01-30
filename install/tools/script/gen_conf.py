#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import sys
import os
import platform
import locale
import stat
import shutil
import re
import string
import shutil
from argparse import ArgumentParser

import glob

console_encoding = sys.getfilesystemencoding()
script_dir = os.path.dirname(os.path.realpath(__file__))

if __name__ == '__main__':
    sys.path.append(script_dir)
    python3_mode = sys.version_info[0] >= 3
    if python3_mode:
        import configparser
    else:
        import ConfigParser as configparser

    import common.print_color
    import common.project_utils as project

    os.chdir(script_dir)
    from mako.template import Template
    from mako.lookup import TemplateLookup
    etc_template_dir = os.path.join(script_dir, 'helper', 'template', 'etc')
    script_template_dir = os.path.join(
        script_dir, 'helper', 'template', 'script')
    project_lookup = TemplateLookup(directories=[
                                    etc_template_dir, script_template_dir], module_directory=os.path.join(script_dir, '.mako_modules'))

    parser = ArgumentParser(usage="usage: %(prog)s [options...]")
    parser.add_argument("-e", "--env-prefix", action='store', dest="env_prefix",
                        default='AUTOBUILD_', help="prefix when read parameters from environment variables")
    parser.add_argument("-c", "--config", action='store', dest="config", default=os.path.join(script_dir,
                                                                                              'config.conf'), help="configure file(default: {0})".format(os.path.join(script_dir, 'config.conf')))
    parser.add_argument("-s", "--set", action='append',
                        dest="set_vars", default=[], help="set configures")
    parser.add_argument("-n", "--number", action='store', dest="reset_number",
                        default=None, type=int, help="set default server numbers")
    parser.add_argument("-i", "--id-offset", action='store', dest="server_id_offset",
                        default=0, type=int, help="set server id offset(default: 0)")

    opts = parser.parse_args()
    if python3_mode:
        config = configparser.ConfigParser(inline_comment_prefixes=('#', ';'))
        config.read(opts.config, encoding="UTF-8")
    else:
        config = configparser.ConfigParser()
        config.read(opts.config)

    project.set_global_opts(config, opts.server_id_offset)

    # all custon environment start with SYSTEM_MACRO_CUSTOM_
    # reset all servers's number
    if opts.reset_number is not None:
        for svr_name in project.get_global_all_services():
            config.set('server.{0}'.format(svr_name),
                       'number', opts.reset_number)

    # set all custom configures
    ext_cmd_rule = re.compile('(.*)\.([^\.]+)=([^=]*)$')
    for cmd in opts.set_vars:
        mat_res = ext_cmd_rule.match(cmd)
        if mat_res:
            config.set(mat_res.group(1), mat_res.group(2), mat_res.group(3))
        else:
            common.print_color.cprintf_stdout([common.print_color.print_style.FC_RED, common.print_color.print_style.FW_BOLD],
                                              'set command {0} invalid, must be SECTION.KEY=VALUE\r\n', cmd)

    # copy script templates
    all_service_temps = {
        'restart_all.sh': {
            'in': os.path.join(script_dir, 'helper', 'template', 'script', 'restart_all.template.sh'),
            'out': os.path.join(script_dir, 'restart_all.sh'),
            'content': []
        },
        'reload_all.sh': {
            'in': os.path.join(script_dir, 'helper', 'template', 'script', 'reload_all.template.sh'),
            'out': os.path.join(script_dir, 'reload_all.sh'),
            'content': []
        },
        'stop_all.sh': {
            'in': os.path.join(script_dir, 'helper', 'template', 'script', 'stop_all.template.sh'),
            'out': os.path.join(script_dir, 'stop_all.sh'),
            'content': [],
            'reverse': True
        }
    }

    for all_svr_temp in all_service_temps:
        all_temp_cfg = all_service_temps[all_svr_temp]
        shutil.copy2(all_temp_cfg['in'], all_temp_cfg['out'])
        os.chmod(all_temp_cfg['out'], stat.S_IRWXU +
                 stat.S_IRWXG + stat.S_IROTH + stat.S_IXOTH)

    def generate_service(svr_name, svr_index, install_prefix, section_name, **ext_options):
        project.set_server_inst(config.items(
            section_name), svr_name, svr_index)
        common.print_color.cprintf_stdout([common.print_color.print_style.FC_YELLOW, common.print_color.print_style.FW_BOLD],
                                          'start to generate etc and script of {0}-{1}\r\n', svr_name, svr_index)

        install_abs_prefix = os.path.normpath(
            os.path.join(script_dir, '..', '..', install_prefix))

        if not os.path.exists(os.path.join(install_abs_prefix, 'etc')):
            os.makedirs(os.path.join(install_abs_prefix, 'etc'))
        if not os.path.exists(os.path.join(install_abs_prefix, 'bin')):
            os.makedirs(os.path.join(install_abs_prefix, 'bin'))

        def generate_template(temp_dir, temp_path, out_dir, out_path, all_content_script=None):
            gen_in_path = os.path.join(temp_dir, temp_path)
            gen_out_path = os.path.join(install_abs_prefix, out_dir, out_path)
            if os.path.exists(gen_in_path):
                svr_tmpl = project_lookup.get_template(temp_path)
                open(gen_out_path, mode='w').write(svr_tmpl.render(
                    project_install_prefix=os.path.relpath(
                        '.', os.path.join(install_prefix, out_dir)),
                    **ext_options
                ))
                os.chmod(gen_out_path, stat.S_IRWXU +
                         stat.S_IRWXG + stat.S_IROTH + stat.S_IXOTH)
                if all_content_script is not None and all_content_script in all_service_temps:
                    all_service_temps[all_content_script]['content'].append("""
# ==================== {0} ==================== 
if [ $# -eq 0 ] || [ "0" == "$(is_in_server_list {1} $*)" ]; then 
    bash {2}
fi
                    """.format(
                        project.get_server_full_name(),
                        project.get_server_full_name(),
                        os.path.relpath(gen_out_path, script_dir)
                    ))

        # etc
        generate_template(etc_template_dir, '{0}.conf'.format(
            svr_name), 'etc', '{0}-{1}.conf'.format(svr_name, svr_index))

        # scripts
        generate_template(script_template_dir, 'start.sh', 'bin',
                          'start-{0}.sh'.format(svr_index), 'restart_all.sh')
        generate_template(script_template_dir, 'stop.sh', 'bin',
                          'stop-{0}.sh'.format(svr_index), 'stop_all.sh')
        generate_template(script_template_dir, 'reload.sh', 'bin',
                          'reload-{0}.sh'.format(svr_index), 'reload_all.sh')
        generate_template(script_template_dir, 'debug.sh',
                          'bin', 'debug-{0}.sh'.format(svr_index))
        generate_template(script_template_dir, 'run.sh',
                          'bin', 'run-{0}.sh'.format(svr_index))

    # parse all services
    atgateway_index = 1 + opts.server_id_offset
    for svr_name in project.get_global_all_services():
        section_name = 'server.{0}'.format(svr_name)
        install_prefix = project.get_global_option(
            section_name, 'install_prefix', svr_name)
        for svr_index in project.get_service_index_range(int(project.get_global_option(section_name, 'number', 0))):
            generate_service(svr_name, svr_index, install_prefix, section_name)
            # atgateway if available
            atgateway_port = project.get_server_option('atgateway_port', None)
            if not atgateway_port is None:
                atgateway_install_prefix = project.get_global_option(
                    'server.atgateway', 'install_prefix', 'atframe/atgateway')
                generate_service('atgateway', atgateway_index, atgateway_install_prefix, section_name,
                                 atgateway_server_name=svr_name,
                                 atgateway_server_index=svr_index
                                 )
                atgateway_index = atgateway_index + 1

    for all_svr_temp in all_service_temps:
        all_temp_cfg = all_service_temps[all_svr_temp]
        if 'reverse' in all_temp_cfg and all_temp_cfg['reverse']:
            all_temp_cfg['content'].reverse()
        open(all_temp_cfg['out'], mode='a').write(os.linesep.join(all_temp_cfg['content']))

    common.print_color.cprintf_stdout(
        [common.print_color.print_style.FC_YELLOW, common.print_color.print_style.FW_BOLD], 'all jobs done.\r\n')
