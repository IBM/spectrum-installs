#!/bin/sh

createIgConsumers() {
	local CONSUMER_BASE_NAME=$1
	local EXECUTION_USER=$2
	local RG_CPU_NAME=$3
	local RG_GPU_NAME=$4
	local ADMIN_USER=$5
	createConsumer "/$CONSUMER_BASE_NAME" "$EXECUTION_USER" $RG_CPU_NAME $RG_GPU_NAME $ADMIN_USER
	createConsumer "/$CONSUMER_BASE_NAME/$CONSUMER_BASE_NAME-MasterServices" "$EXECUTION_USER" $RG_CPU_NAME $RG_GPU_NAME $ADMIN_USER
	createConsumer "/$CONSUMER_BASE_NAME/$CONSUMER_BASE_NAME-NonMasterWorkload" "$EXECUTION_USER" $RG_CPU_NAME $RG_GPU_NAME $ADMIN_USER
	createConsumer "/$CONSUMER_BASE_NAME/$CONSUMER_BASE_NAME-NonMasterWorkload/$CONSUMER_BASE_NAME-Drivers" "$EXECUTION_USER" $RG_CPU_NAME $RG_GPU_NAME $ADMIN_USER
	createConsumer "/$CONSUMER_BASE_NAME/$CONSUMER_BASE_NAME-NonMasterWorkload/$CONSUMER_BASE_NAME-Executors" "$EXECUTION_USER" $RG_CPU_NAME $RG_GPU_NAME $ADMIN_USER
}

updateResourcePlanIgConsumers() {
	local CONSUMER_BASE_NAME=$1
	local RESPLAN_SOURCE=/tmp/resource-plan-source-`date | md5sum | head -c8`.xml
	local RESPLAN_UPDATED=/tmp/resource-plan-updated-`date | md5sum | head -c8`.xml
	authenticate
	log "Updating resource plan for Instance Group in consumer $CONSUMER_BASE_NAME"
	egosh consumer exportresplan $RESPLAN_SOURCE 2>&1 | tee -a $LOG_FILE
	if [ ! -f $RESPLAN_SOURCE ]
	then
		log "Failed to export resource plan, aborting the update of resource plan." ERROR
	else
		$PYTHON_BIN $UPDATE_RP_SCRIPT -i $RESPLAN_SOURCE -o $RESPLAN_UPDATED -c "$CONSUMER_BASE_NAME-NonMasterWorkload,$CONSUMER_BASE_NAME-Executors" 2>&1 | tee -a $LOG_FILE
		sed -i 's#ParameterName="DistributeBy" />#ParameterName="DistributeBy"/>#g' $RESPLAN_UPDATED 2>&1 | tee -a $LOG_FILE
		if [ ${PIPESTATUS[0]} -ne 0 ]
		then
			log "Failed to modify resource plan, aborting the update of resource plan." ERROR
		else
			egosh consumer applyresplan $RESPLAN_UPDATED 2>&1 | tee -a $LOG_FILE
			if [ ${PIPESTATUS[0]} -ne 0 ]
			then
				log "Failed to apply updated resource plan." ERROR
			else
				log "Resource plan modified successfully" SUCCESS
			fi
		fi
		[[ -f $RESPLAN_SOURCE ]] && rm -f $RESPLAN_SOURCE 2>&1 | tee -a $LOG_FILE
		[[ -f $RESPLAN_UPDATED ]] && rm -f $RESPLAN_UPDATED 2>&1 | tee -a $LOG_FILE
	fi
}

findIgPort() {
	local __resultvar=$1
	local PORT_NAME=$2
	local PORT_DEFAULT=$3
	getRestUrls
	local PORT=`curl -s -S -k -u $EGO_ADMIN_USERNAME:$EGO_ADMIN_PASSWORD -H "Accept:application/json" -X GET ${ASCD_REST_BASE_URL}conductor/v1/instances/parametersinuse |$PYTHON_BIN -c $'import sys,json\nparams=json.load(sys.stdin)\nportMax=0\nfor paramRG in params:\n  if len(paramRG["parametersinuse"]["'$PORT_NAME$'"])>0 and max(paramRG["parametersinuse"]["'$PORT_NAME$'"])>portMax: portMax=max(paramRG["parametersinuse"]["'$PORT_NAME$'"])\nif int(portMax)>0: print int(portMax)+1\nelse: print '$PORT_DEFAULT`
	eval $__resultvar="'$PORT'"
}

findIgPorts() {
	findIgPort SPARK_MASTER_PORT "SPARK_MASTER_PORT" 7077
	findIgPort SPARK_MASTER_WEBUI_PORT "SPARK_MASTER_WEBUI_PORT" 8780
	findIgPort SPARK_MASTER_REST_PORT "spark.master.rest.port" 6066
	findIgPort SPARK_SHUFFLE_SERVICE_PORT "spark.shuffle.service.port" 7337
	findIgPort SPARK_HISTORY_UI_PORT "spark.history.ui.port" 18080
	findIgPort SPARK_EGO_LOGSERVICE_PORT "SPARK_EGO_LOGSERVICE_PORT" 28082
}

createIgSparkJupyter() {
	local IG_PROFILE_TEMPLATE=$1
	local CONSUMER_BASE=$2
	local IG_NAME=$3
	local EXECUTION_USER=$4
	local DEPLOYMENT_BASEDIR=$5
	local SPARKHA_BASEDIR=$6
	local SPARKHISTORY_BASE_DIR=$7
	local NOTEBOOK_BASE_DIR=$8
	local RG_CPU=$9
	local RG_GPU=${10}
	local ANACONDA_INSTANCE_NAME=${11}
	local CONDA_ENV_NAME=${12}
	local SPARKSHUFFLE_SERVICE_ENABLED=${13} # Expect "true" or "false"
	if [ "$SPARKSHUFFLE_SERVICE_ENABLED" == "false" ]
	then
		local SPARKSHUFFLE_BASE_DIR=${14}
	fi
	local TMP_IG_PROFILE=/tmp/ig-profile-`date | md5sum | head -c8`.json
	local CONSUMER_BASE=${CONSUMER_BASE//\//\\\/}
	local DEPLOYMENT_BASEDIR_TMP=${DEPLOYMENT_BASEDIR//\//\\\/}
	local SPARKHA_BASEDIR_TMP=${SPARKHA_BASEDIR//\//\\\/}
	local SPARKHISTORY_BASE_DIR_TMP=${SPARKHISTORY_BASE_DIR//\//\\\/}
	local NOTEBOOK_BASE_DIR_TMP=${NOTEBOOK_BASE_DIR//\//\\\/}
	getRestUrls
	findIgPorts
	getAnacondaInstanceUUID $ANACONDA_INSTANCE_NAME
	log "Preparing IG profile $TMP_IG_PROFILE using template $IG_PROFILE_TEMPLATE"
	cp $IG_PROFILE_TEMPLATE $TMP_IG_PROFILE 2>&1 | tee -a $LOG_FILE
	sed -i 's/##CONSUMER_BASE##/'$CONSUMER_BASE'/g' $TMP_IG_PROFILE 2>&1 | tee -a $LOG_FILE
	sed -i 's/##IG_NAME##/'$IG_NAME'/g' $TMP_IG_PROFILE 2>&1 | tee -a $LOG_FILE
	sed -i 's/##RG_CPU##/'$RG_CPU'/g' $TMP_IG_PROFILE 2>&1 | tee -a $LOG_FILE
	if [ "$RG_CPU" == "$RG_GPU" ]
	then
		sed -i 's/"executor_gpu_rg_param":"##RG_GPU##"\,//g' $TMP_IG_PROFILE 2>&1 | tee -a $LOG_FILE
	else
		sed -i 's/##RG_GPU##/'$RG_GPU'/g' $TMP_IG_PROFILE 2>&1 | tee -a $LOG_FILE
	fi
	sed -i 's/##EXECUTION_USER##/'$EXECUTION_USER'/g' $TMP_IG_PROFILE 2>&1 | tee -a $LOG_FILE
	sed -i 's/##DEPLOYMENT_BASEDIR##/'$DEPLOYMENT_BASEDIR_TMP'/g' $TMP_IG_PROFILE 2>&1 | tee -a $LOG_FILE
	sed -i 's/##SPARKHA_BASEDIR##/'$SPARKHA_BASEDIR_TMP'/g' $TMP_IG_PROFILE 2>&1 | tee -a $LOG_FILE
	sed -i 's/##SPARKHISTORY_BASE_DIR##/'$SPARKHISTORY_BASE_DIR_TMP'/g' $TMP_IG_PROFILE 2>&1 | tee -a $LOG_FILE
	sed -i 's/##SPARK_MASTER_PORT##/'$SPARK_MASTER_PORT'/g' $TMP_IG_PROFILE 2>&1 | tee -a $LOG_FILE
	sed -i 's/##SPARK_MASTER_WEBUI_PORT##/'$SPARK_MASTER_WEBUI_PORT'/g' $TMP_IG_PROFILE 2>&1 | tee -a $LOG_FILE
	sed -i 's/##SPARK_MASTER_REST_PORT##/'$SPARK_MASTER_REST_PORT'/g' $TMP_IG_PROFILE 2>&1 | tee -a $LOG_FILE
	if [ "$SPARKSHUFFLE_SERVICE_ENABLED" == "true" ]
	then
		sed -i 's/##SPARK_SHUFFLE_SERVICE_PORT##/'$SPARK_SHUFFLE_SERVICE_PORT'/g' $TMP_IG_PROFILE 2>&1 | tee -a $LOG_FILE
	else
		local SPARK_LOCAL_DIR=$SPARKSHUFFLE_BASE_DIR/$IG_NAME
		sed -i 's#"spark.shuffle.service.port.*$#"spark.shuffle.service.enabled": "false",\n              "spark.local.dir": "'$SPARK_LOCAL_DIR'",#' $TMP_IG_PROFILE 2>&1 | tee -a $LOG_FILE
	fi
	sed -i 's/##SPARK_HISTORY_UI_PORT##/'$SPARK_HISTORY_UI_PORT'/g' $TMP_IG_PROFILE 2>&1 | tee -a $LOG_FILE
	sed -i 's/##SPARK_EGO_LOGSERVICE_PORT##/'$SPARK_EGO_LOGSERVICE_PORT'/g' $TMP_IG_PROFILE 2>&1 | tee -a $LOG_FILE
	sed -i 's/##NOTEBOOK_BASE_DIR##/'$NOTEBOOK_BASE_DIR_TMP'/g' $TMP_IG_PROFILE 2>&1 | tee -a $LOG_FILE
	sed -i 's/##CONDA_ENV_NAME##/'$CONDA_ENV_NAME'/g' $TMP_IG_PROFILE 2>&1 | tee -a $LOG_FILE
	sed -i 's/##ANACONDA_INSTANCE_NAME##/'$ANACONDA_INSTANCE_NAME'/g' $TMP_IG_PROFILE 2>&1 | tee -a $LOG_FILE
	sed -i 's/##ANACONDA_INSTANCE_UUID##/'$ANACONDA_INSTANCE_UUID'/g' $TMP_IG_PROFILE 2>&1 | tee -a $LOG_FILE
	registerIg $TMP_IG_PROFILE
	rm -f $TMP_IG_PROFILE
	deployIg $IG_UUID
	startIg $IG_UUID
}

registerIg() {
	local IG_PROFILE=$1
	getRestUrls
	log "Registering Instance Group"
	local IG_REGISTRATION_OUTPUT=`curl -s -S -k -u $EGO_ADMIN_USERNAME:$EGO_ADMIN_PASSWORD -H "Content-Type:application/json" -H "Accept:application/json" -X POST --data-binary @$IG_PROFILE ${ASCD_REST_BASE_URL}conductor/v1/instances/`
	export IG_UUID=`echo $IG_REGISTRATION_OUTPUT | sed -e 's/"//g'`
	if [ ${#IG_UUID} -ne 36 ]
	then
		log "Failed to register Instance Group, aborting. Output of registration tentative:" ERROR
		log "$IG_REGISTRATION_OUTPUT" ERROR
		exit 1
	fi
	log "Instance Group registered successfully" SUCCESS
}

deployIg() {
	local IG_UUID=$1
	getRestUrls
	log "Deploying Instance Group"
	local CURL_OUT=`curl -s -S -k -w "%{http_code}" -u $EGO_ADMIN_USERNAME:$EGO_ADMIN_PASSWORD -X PUT ${ASCD_REST_BASE_URL}conductor/v1/instances/$IG_UUID/deploy?timeout=$IG_DEPLOY_TIMEOUT`
	local RESPONSE=${CURL_OUT:0:(-3)}
	local HTTP_CODE=${CURL_OUT:(-3)}
	if [ "$HTTP_CODE" == "204" ]
	then
		log "Instance Group $IG_UUID deployment started successfully" SUCCESS
	else
		log "Failed to start deployment of Instance Group $IG_UUID (HTTP CODE $HTTP_CODE), aborting. Output of deployment tentative:" ERROR
		log "$RESPONSE" ERROR
		exit 1
	fi
	getIgState $IG_UUID
	while [ "$IG_STATE" != "READY" ]
	do
		if [ "$IG_STATE" == "DEPLOY_ERROR" ]
		then
			log "Deployment failed (Instance Group state: $IG_STATE)" ERROR
			exit 1
		fi
		log "Instance Group deploying (state: $IG_STATE) ..."
		sleep $STATUS_CHECK_WAITTIME
		getIgState $IG_UUID
	done
	log "Instance Group deployed successfully" SUCCESS
}

startIg() {
	local IG_UUID=$1
	getRestUrls
	log "Starting Instance Group"
	local CURL_OUT=`curl -s -S -k -w "%{http_code}" -u $EGO_ADMIN_USERNAME:$EGO_ADMIN_PASSWORD -X PUT ${ASCD_REST_BASE_URL}conductor/v1/instances/$IG_UUID/start`
	local RESPONSE=${CURL_OUT:0:(-3)}
	local HTTP_CODE=${CURL_OUT:(-3)}
	if [ "$HTTP_CODE" == "204" ]
	then
		log "Instance Group $IG_UUID start operation started successfully" SUCCESS
	else
		log "Failed to start Instance Group $IG_UUID (HTTP CODE $HTTP_CODE). Output of start tentative:" WARNING
		log "$RESPONSE" WARNING
	fi
	getIgState $IG_UUID
	while [ "$IG_STATE" != "STARTED" ]
	do
		if [ "$IG_STATE" == "ERROR" ]
		then
			log "Failed to start Instance Group (state: $IG_STATE)" WARNING
		fi
		log "Instance Group starting (state: $IG_STATE) ..."
		sleep $STATUS_CHECK_WAITTIME
		getIgState $IG_UUID
	done
	log "Instance Group started" SUCCESS
}

stopIg() {
	local IG_UUID=$1
	getRestUrls
	log "Stopping Instance Group"
	local CURL_OUT=`curl -s -S -k -w "%{http_code}" -u $EGO_ADMIN_USERNAME:$EGO_ADMIN_PASSWORD -X PUT ${ASCD_REST_BASE_URL}conductor/v1/instances/$IG_UUID/stop`
	local RESPONSE=${CURL_OUT:0:(-3)}
	local HTTP_CODE=${CURL_OUT:(-3)}
	if [ "$HTTP_CODE" == "204" ]
	then
		log "Instance Group $IG_UUID stop operation started successfully" SUCCESS
	else
		log "Failed to stop Instance Group $IG_UUID (HTTP CODE $HTTP_CODE). Output of stop tentative:" WARNING
		log "$RESPONSE" WARNING
	fi
	getIgState $IG_UUID
	while [ "$IG_STATE" != "READY" ]
	do
		log "Instance Group stopping (state: $IG_STATE) ..."
		sleep $STATUS_CHECK_WAITTIME
		getIgState $IG_UUID
	done
	log "Instance Group stopped" SUCCESS
}

createIgNotebookInstance() {
	local NOTEBOOK_INSTANCE_USER=$1
	local IG_UUID=$2
	local NOTEBOOK_NAME=$3
	local NOTEBOOK_VERSION=$4
	getRestUrls
	local CURL_OUT=`curl -s -S -k -w "%{http_code}" -u $EGO_ADMIN_USERNAME:$EGO_ADMIN_PASSWORD -H "Content-Type:application/json" -H "Accept:application/json" -X POST --data-binary '{"userlist": [ "'$NOTEBOOK_INSTANCE_USER'" ], "notebookinfolist": [ { "notebook": "'$NOTEBOOK_NAME'", "version": "'$NOTEBOOK_VERSION'" } ] }' ${ASCD_REST_BASE_URL}conductor/v1/instances/$IG_UUID/notebooks`
	local RESPONSE=${CURL_OUT:0:(-3)}
	local HTTP_CODE=${CURL_OUT:(-3)}
	if [ "$HTTP_CODE" == "201" ]
	then
		log "Notebook instance for user $NOTEBOOK_INSTANCE_USER on Instance Group $IG_UUID and notebook $NOTEBOOK_NAME $NOTEBOOK_VERSION created successfully" SUCCESS
	else
		log "Failed to create Notebook instance for user $NOTEBOOK_INSTANCE_USER on Instance Group $IG_UUID and notebook $NOTEBOOK_NAME $NOTEBOOK_VERSION (HTTP CODE $HTTP_CODE). Output of creation tentative:" WARNING
		log "$RESPONSE" WARNING
	fi
}

getIgState() {
	local IG_UUID=$1
	getRestUrls
	export IG_STATE=`curl -s -S -k -u $EGO_ADMIN_USERNAME:$EGO_ADMIN_PASSWORD -H "Accept:application/json" -X GET ${ASCD_REST_BASE_URL}conductor/v1/instances/?id=$IG_UUID |$PYTHON_BIN -c 'import sys, json; print json.load(sys.stdin)[0]["state"]'`
}

getIgUUID() {
	local IG_NAME=$1
	getRestUrls
	export IG_UUID=`curl -s -S -k -u $EGO_ADMIN_USERNAME:$EGO_ADMIN_PASSWORD -H "Accept:application/json" -X GET ${ASCD_REST_BASE_URL}conductor/v1/instances/?name=$IG_NAME | $PYTHON_BIN -c 'import sys, json; print json.load(sys.stdin)[0]["id"]["uuid"]'`
}
