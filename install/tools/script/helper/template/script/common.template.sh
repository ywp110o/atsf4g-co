#!/usr/bin/env bash
<%!
    import common.project_utils as project
%>
SCRIPT_DIR="$( cd "$( dirname "$0" )" && pwd )";
SCRIPT_DIR="$( readlink -f $SCRIPT_DIR )";
cd "$SCRIPT_DIR";

SERVER_NAME="${project.get_server_name()}";
SERVERD_NAME="$SERVER_NAME";
SERVER_FULL_NAME="${project.get_server_full_name()}";
SERVER_BUS_ID=${hex(project.get_server_id())};
export PROJECT_INSTALL_DIR=$(cd ${project_install_prefix} && pwd);

source "$PROJECT_INSTALL_DIR/tools/script/common/common.sh";

if [ ! -e "$SERVERD_NAME" ]; then
    SERVERD_NAME="${project.get_server_name()}d";
fi

if [ ! -e "$SERVERD_NAME" ]; then
    ErrorMsg "Executable $SERVER_NAME or $SERVERD_NAME not found, run $@ failed";
    exit 1;
fi
SERVER_PID_FILE_NAME="$SERVER_FULL_NAME.pid";

export LD_LIBRARY_PATH=$PROJECT_INSTALL_DIR/lib:$PROJECT_INSTALL_DIR/tools/shared:$LD_LIBRARY_PATH ;

PROFILE_DIR="$PROJECT_INSTALL_DIR/profile";
export GCOV_PREFIX="$PROFILE_DIR/gcov";
export GCOV_PREFIX_STRIP=16 ;
mkdir -p "$GCOV_PREFIX";
<%
import os
server_preload_scripts=[]
if project.get_global_option_bool('jemalloc', 'malloc', False):
    jemalloc_profile_dir = str(project.get_global_option('jemalloc', 'profile_dir', 'profile'))
    jemalloc_profile_dir = os.path.join('$PROJECT_INSTALL_DIR', jemalloc_profile_dir)
    server_preload_scripts.append('mkdir -p "{0}" ;'.format(jemalloc_profile_dir))
    server_profile_dir = os.path.join(jemalloc_profile_dir, project.get_server_name())

    jemalloc_path = '$PROJECT_INSTALL_DIR/tools/shared/libjemalloc.so'
    jemalloc_options = ''
    jemalloc_heap_check = int(project.get_global_option('jemalloc', 'heap_check', 0))
    if jemalloc_heap_check > 0:
        jemalloc_options = 'prof_leak:true,lg_prof_sample:{0}'.format(jemalloc_heap_check)
    jemalloc_heap_profile = str(project.get_global_option('jemalloc', 'heap_profile', ''))
    if len(jemalloc_heap_profile) > 0:
        jemalloc_options = jemalloc_options + ',prof:true,prof_prefix:{0}'.format(os.path.join(server_profile_dir, jemalloc_heap_profile))
        server_preload_scripts.append('mkdir -p "{0}" ;'.format(server_profile_dir))
    jemalloc_other_options = project.get_global_option('jemalloc', 'other_malloc_conf', '')
    if len(jemalloc_other_options) > 0:
        jemalloc_options = jemalloc_options + ',' + jemalloc_other_options
    if jemalloc_options[0:1] == ',':
        jemalloc_options = jemalloc_options[1:]
    
    server_preload_scripts.append('export MALLOC_CONF="{0}" ;'.format(jemalloc_options))
    server_preload_scripts.append('if [ -e "{0}" ]; then'.format(jemalloc_path))
    server_preload_scripts.append('    export LD_PRELOAD="{0}" ;'.format(jemalloc_path))
    server_preload_scripts.append('fi')
elif project.get_global_option_bool('gperftools', 'malloc', False):
    gperftools_profile_dir = str(project.get_global_option('gperftools', 'profile_dir', 'profile'))
    gperftools_profile_dir = os.path.join('$PROJECT_INSTALL_DIR', gperftools_profile_dir)
    server_preload_scripts.append('mkdir -p "{0}" ;'.format(gperftools_profile_dir))
    server_profile_dir = os.path.join(gperftools_profile_dir, project.get_server_name())

    tcmalloc_path = '$PROJECT_INSTALL_DIR/tools/shared/libtcmalloc_minimal.so'
    gperftools_heap_check = str(project.get_global_option('gperftools', 'heap_check', ''))
    gperftools_heap_profile = str(project.get_global_option('gperftools', 'heap_profile', ''))
    gperftools_cpu_profile = str(project.get_global_option('gperftools', 'cpu_profile', ''))
    if len(gperftools_heap_check) > 0 or len(gperftools_heap_profile) > 0:
        tcmalloc_path = '$PROJECT_INSTALL_DIR/tools/shared/libtcmalloc.so'
    if len(gperftools_cpu_profile) > 0:
        tcmalloc_path = '$PROJECT_INSTALL_DIR/tools/shared/libtcmalloc_and_profiler.so'
        server_preload_scripts.append('mkdir -p "{0}" ;'.format(server_profile_dir))
        server_preload_scripts.append('export CPUPROFILE="{0}" ;'.format(os.path.join(server_profile_dir, gperftools_cpu_profile)))
    if len(gperftools_heap_profile) > 0:
        server_preload_scripts.append('mkdir -p "{0}" ;'.format(server_profile_dir))
        server_preload_scripts.append('export HEAPPROFILE="{0}" ;'.format(os.path.join(server_profile_dir, gperftools_heap_profile)))
    if len(gperftools_heap_check) > 0:
        server_preload_scripts.append('export HEAPCHECK={0} ;'.format(gperftools_heap_check))

    server_preload_scripts.append('if [ -e "{0}" ]; then'.format(tcmalloc_path))
    server_preload_scripts.append('    export LD_PRELOAD="{0}" ;'.format(tcmalloc_path))
    server_preload_scripts.append('fi')
%>
${os.linesep.join(server_preload_scripts)}