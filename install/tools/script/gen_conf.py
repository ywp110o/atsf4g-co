#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import sys
import os, platform, locale, stat
import shutil, re, string, shutil
from optparse import OptionParser

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
    script_template_dir = os.path.join(script_dir, 'helper', 'template', 'script')
    project_lookup = TemplateLookup(directories=[etc_template_dir, script_template_dir], module_directory = os.path.join(script_dir, '.mako_modules'))

    parser = OptionParser("usage: %prog [options...]")
    parser.add_option("-e", "--env-prefix", action='store', dest="env_prefix", default='AUTOBUILD_', help="prefix when read parameters from environment variables")
    parser.add_option("-c", "--config", action='store', dest="config", default=os.path.join(script_dir, 'config.conf'), help="configure file(default: {0})".format(os.path.join(script_dir, 'config.conf')))
    parser.add_option("-s", "--set", action='append' , dest="set_vars", default=[], help="set configures")
    parser.add_option("-n", "--number", action='store' , dest="reset_number", default=None, type='int', help="set default server numbers")
    parser.add_option("-i", "--id-offset", action='store' , dest="server_id_offset", default=0, type='int', help="set server id offset(default: 0)")

    (opts, args) = parser.parse_args()
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
            config.set('server.{0}'.format(svr_name), 'number', opts.reset_number)

    # set all custom configures
    ext_cmd_rule = re.compile('(.*)\.([^\.]+)=([^=]*)$')
    for cmd in opts.set_vars:
        mat_res = ext_cmd_rule.match(cmd)
        if mat_res:
            config.set(mat_res.group(1), mat_res.group(2), mat_res.group(3))
        else:
            common.print_color.cprintf_stdout([common.print_color.print_style.FC_RED, common.print_color.print_style.FW_BOLD], 'set command {0} invalid, must be SECTION.KEY=VALUE\r\n', cmd)
    
    # copy script templates
    restart_all_script = os.path.join(script_dir, 'restart_all.sh')
    reload_all_script = os.path.join(script_dir, 'reload_all.sh')
    stop_all_script = os.path.join(script_dir, 'stop_all.sh')
    shutil.copy2(
        os.path.join(script_dir, 'helper', 'template', 'script', 'restart_all.template.sh'), 
        restart_all_script
    )
    os.chmod(restart_all_script, stat.S_IRWXU + stat.S_IRWXG + stat.S_IROTH + stat.S_IXOTH)
    shutil.copy2(
        os.path.join(script_dir, 'helper', 'template', 'script', 'reload_all.template.sh'), 
        reload_all_script
    )
    os.chmod(reload_all_script, stat.S_IRWXU + stat.S_IRWXG + stat.S_IROTH + stat.S_IXOTH)
    shutil.copy2(
        os.path.join(script_dir, 'helper', 'template', 'script', 'stop_all.template.sh'), 
        stop_all_script
    )
    os.chmod(stop_all_script, stat.S_IRWXU + stat.S_IRWXG + stat.S_IROTH + stat.S_IXOTH)
    restart_all_content = []
    reload_all_content = []
    stop_all_content = []
    
    def generate_service(svr_name, svr_index, install_prefix, section_name, **ext_options):
        project.set_server_inst(config.items(section_name), svr_name, svr_index)
        common.print_color.cprintf_stdout([common.print_color.print_style.FC_YELLOW, common.print_color.print_style.FW_BOLD], 'start to generate etc and script of {0}-{1}\r\n', svr_name, svr_index)

        etc_in_name = '{0}.conf'.format(svr_name)
        etc_out_name = '{0}-{1}.conf'.format(svr_name, svr_index)
        start_script_name = 'start-{0}.sh'.format(svr_index)
        stop_script_name = 'stop-{0}.sh'.format(svr_index)
        reload_script_name = 'reload-{0}.sh'.format(svr_index)
        debug_script_name = 'debug-{0}.sh'.format(svr_index)
        install_abs_prefix = os.path.normpath(os.path.join(script_dir, '..', '..', install_prefix))

        if not os.path.exists(os.path.join(install_abs_prefix, 'etc')):
            os.makedirs(os.path.join(install_abs_prefix, 'etc'))
        if not os.path.exists(os.path.join(install_abs_prefix, 'bin')):
            os.makedirs(os.path.join(install_abs_prefix, 'bin'))
        # etc
        gen_in_path = os.path.join(etc_template_dir, etc_in_name)
        gen_out_path = os.path.join(install_abs_prefix, 'etc', etc_out_name)
        if os.path.exists(gen_in_path):
            svr_tmpl = project_lookup.get_template(etc_in_name)
            open(gen_out_path, mode='w').write(svr_tmpl.render(
                project_install_prefix=os.path.relpath('.', os.path.join(install_prefix, 'etc')),
                **ext_options
            ))
            os.chmod(gen_out_path, stat.S_IRWXU + stat.S_IRWXG + stat.S_IROTH + stat.S_IXOTH)
        # start script
        gen_in_path = os.path.join(script_template_dir, 'start.sh')
        gen_out_path = os.path.join(install_abs_prefix, 'bin', start_script_name)
        if os.path.exists(gen_in_path):
            svr_tmpl = project_lookup.get_template('start.sh')
            open(gen_out_path, mode='w').write(svr_tmpl.render(
                project_install_prefix=os.path.relpath('.', os.path.join(install_prefix, 'bin')),
                **ext_options
            ))
            os.chmod(gen_out_path, stat.S_IRWXU + stat.S_IRWXG + stat.S_IROTH + stat.S_IXOTH)
            restart_all_content.append("""
# ==================== {0} ==================== 
if [ $# -eq 0 ] || [ "0" == "$(is_in_server_list {1} $*)" ]; then 
    bash {2}
fi
            """.format(
                project.get_server_full_name(), 
                project.get_server_full_name(), 
                os.path.relpath(gen_out_path, script_dir)
            ))

        # stop script
        gen_in_path = os.path.join(script_template_dir, 'stop.sh')
        gen_out_path = os.path.join(install_abs_prefix, 'bin', stop_script_name)
        if os.path.exists(gen_in_path):
            svr_tmpl = project_lookup.get_template('stop.sh')
            open(gen_out_path, mode='w').write(svr_tmpl.render(
                project_install_prefix=os.path.relpath('.', os.path.join(install_prefix, 'bin')),
                **ext_options
            ))
            os.chmod(gen_out_path, stat.S_IRWXU + stat.S_IRWXG + stat.S_IROTH + stat.S_IXOTH)
            stop_all_content.append("""
# ==================== {0} ==================== 
if [ $# -eq 0 ] || [ "0" == "$(is_in_server_list {1} $*)" ]; then 
    bash {2}
fi
            """.format(
                project.get_server_full_name(), 
                project.get_server_full_name(), 
                os.path.relpath(gen_out_path, script_dir)
            ))

        # reload script
        gen_in_path = os.path.join(script_template_dir, 'reload.sh')
        gen_out_path = os.path.join(install_abs_prefix, 'bin', reload_script_name)
        if os.path.exists(gen_in_path):
            svr_tmpl = project_lookup.get_template('reload.sh')
            open(gen_out_path, mode='w').write(svr_tmpl.render(
                project_install_prefix=os.path.relpath('.', os.path.join(install_prefix, 'bin')),
                **ext_options
            ))
            os.chmod(gen_out_path, stat.S_IRWXU + stat.S_IRWXG + stat.S_IROTH + stat.S_IXOTH)
            reload_all_content.append("""
# ==================== {0} ==================== 
if [ $# -eq 0 ] || [ "0" == "$(is_in_server_list {1} $*)" ]; then 
    bash {2}
fi
            """.format(
                project.get_server_full_name(), 
                project.get_server_full_name(), 
                os.path.relpath(gen_out_path, script_dir)
            ))
            
        # debug script
        gen_in_path = os.path.join(script_template_dir, 'debug.sh')
        gen_out_path = os.path.join(install_abs_prefix, 'bin', debug_script_name)
        if os.path.exists(gen_in_path):
            svr_tmpl = project_lookup.get_template('debug.sh')
            open(gen_out_path, mode='w').write(svr_tmpl.render(
                project_install_prefix=os.path.relpath('.', os.path.join(install_prefix, 'bin')),
                **ext_options
            ))
            os.chmod(gen_out_path, stat.S_IRWXU + stat.S_IRWXG + stat.S_IROTH + stat.S_IXOTH)
   
    # parse all services
    atgateway_index = 1 + opts.server_id_offset
    for svr_name in project.get_global_all_services():
        section_name = 'server.{0}'.format(svr_name)
        install_prefix = project.get_global_option(section_name, 'install_prefix', svr_name)
        for svr_index in project.get_service_index_range(int(project.get_global_option(section_name, 'number', 0))):
            generate_service(svr_name, svr_index, install_prefix, section_name)
            # atgateway if available
            atgateway_port = project.get_server_option('atgateway_port', None)
            if not atgateway_port is None:
                atgateway_install_prefix = project.get_global_option('server.atgateway', 'install_prefix', 'atframe/atgateway')
                generate_service('atgateway', atgateway_index, atgateway_install_prefix, section_name,
                    atgateway_server_name = svr_name,
                    atgateway_server_index = svr_index
                )
                atgateway_index = atgateway_index + 1

    open(restart_all_script, mode='a').write(os.linesep.join(restart_all_content))
    open(reload_all_script, mode='a').write(os.linesep.join(reload_all_content))
    stop_all_content.reverse()
    open(stop_all_script, mode='a').write(os.linesep.join(stop_all_content))
    common.print_color.cprintf_stdout([common.print_color.print_style.FC_YELLOW, common.print_color.print_style.FW_BOLD], 'all jobs done.\r\n')
        
