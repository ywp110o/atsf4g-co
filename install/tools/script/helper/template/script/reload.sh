<%!
    import common.project_utils as project
%><%include file="common.template.sh" />

./$SERVERD_NAME -id $SERVER_BUS_ID -c ../etc/$SERVER_FULL_NAME.conf -p $SERVER_PID_FILE_NAME reload

export LD_PRELOAD=;

if [ $? -ne 0 ]; then
	ErrorMsg "send reload command to $full_server_name failed.";
	exit $?;
fi

NoticeMsg "reload $SERVER_FULL_NAME done.";