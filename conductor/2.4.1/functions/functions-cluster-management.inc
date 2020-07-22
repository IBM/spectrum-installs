#!/bin/sh

authenticate() {
	source $INSTALL_DIR/profile.platform
	egosh client whoami >/dev/null 2>&1
	if [ $? -ne 0 ]
	then
		egosh user logon -u $EGO_ADMIN_USERNAME -x $EGO_ADMIN_PASSWORD >/dev/null 2>&1
		local CODE=$?
		if [ $CODE -ne 0 ]
		then
			log "Cannot authenticate on EGO (error code: $CODE)" ERROR
			exit 1
		fi
	fi
}

restartCluster() {
	log "Stop EGO services"
	egosh service stop all >/dev/null
	log "Wait for EGO services to be stopped"
	waitForEgoServicesStopped
	log "Restart EGO"
	egosh ego restart -f all 2>&1 | tee -a $LOG_FILE
	log "Wait $EGO_SHUTDOWN_WAITTIME seconds to make sure all EGO processes restarted"
	sleep $EGO_SHUTDOWN_WAITTIME
	log "Wait for cluster to start"
	waitForClusterUp
}

waitForClusterUp() {
	source $INSTALL_DIR/profile.platform
	egosh user logon -u $EGO_ADMIN_USERNAME -x $EGO_ADMIN_PASSWORD >/dev/null 2>&1
	local CODE=$?
	while [ $CODE -ne 0 ]
	do
		log "Cluster not started ..."
		sleep $STATUS_CHECK_WAITTIME
		egosh user logon -u $EGO_ADMIN_USERNAME -x $EGO_ADMIN_PASSWORD >/dev/null 2>&1
		local CODE=$?
	done
}

waitForEgoServiceStarted() {
	local SERVICE_NAME=$1
	authenticate
	local SERVICE_STATE=`egosh service list -ll | grep \"$SERVICE_NAME\" | awk -F"," '{print $7}' | sed -e 's/\"//g'`
	while [ "$SERVICE_STATE" != "STARTED" ]
	do
		if [ "$SERVICE_STATE" == "ERROR" ]
		then
			log "Service $SERVICE_NAME is in ERROR, aborting" ERROR
			exit 1
		fi
		log "Service $SERVICE_NAME not started (state: $SERVICE_STATE) ..."
		sleep $STATUS_CHECK_WAITTIME
		local SERVICE_STATE=`egosh service list -ll | grep \"$SERVICE_NAME\" | awk -F"," '{print $7}' | sed -e 's/\"//g'`
	done
}

waitForEgoServicesStopped() {
	authenticate
	local SERVICES_STATES=`egosh service list -ll | sed -e 1d | awk -F"," '{print $7}' | sed -e 's/\"//g'`
	for SERVICE_STATE in $SERVICES_STATES
	do
		if [ "$SERVICE_STATE" != "DEFINED" ]
		then
			log "At least 1 service is not stopped ..."
			sleep $STATUS_CHECK_WAITTIME
			local SERVICES_STATES=`egosh service list -ll | sed -e 1d | awk -F"," '{print $7}' | sed -e 's/\"//g'`
			break
		fi
	done
}

waitForClientURL() {
	local CLIENT_NAME=$1
	authenticate
	egosh client view $CLIENT_NAME 2>/dev/null | awk '/DESCRIPTION/ {print $2}' | grep http >/dev/null 2>&1
	local CODE=$?
	while [ $CODE -ne 0 ]
	do
		log "URL for client $CLIENT_NAME not available ..."
		sleep $STATUS_CHECK_WAITTIME
		egosh client view $CLIENT_NAME 2>/dev/null | awk '/DESCRIPTION/ {print $2}' | grep http >/dev/null 2>&1
		local CODE=$?
	done
}

waitForRestUrlsUp(){
	log "Wait for EGO Service REST to start"
	waitForEgoServiceStarted REST
	log "Wait for EGO Service ascd to start"
	waitForEgoServiceStarted ascd
	log "Wait for EGO REST URL to be available"
	waitForClientURL REST_BASE_URL_1
	log "Wait for ASCD REST URL to be available"
	waitForClientURL ASCD_REST_BASE_URL_1
}

waitForGuiUp() {
	log "Wait for EGO Service REST to start"
	waitForEgoServiceStarted REST
	log "Wait for EGO Service WEBGUI to start"
	waitForEgoServiceStarted WEBGUI
	log "Wait for EGO REST URL to be available"
	waitForClientURL REST_BASE_URL_1
	log "Wait for WEBGUI URL to be available"
	waitForClientURL GUIURL_1
}

getRestUrls() {
	if [ "$EGO_REST_BASE_URL" == "" -o "$ASCD_REST_BASE_URL" == "" ]
	then
		authenticate
		export EGO_REST_BASE_URL=`egosh client view REST_BASE_URL_1 | awk '/DESCRIPTION/ {print $2}'`
		export ASCD_REST_BASE_URL=`egosh client view ASCD_REST_BASE_URL_1 | awk '/DESCRIPTION/ {print $2}'`
	fi
}

createUser() {
	local USERNAME=$1
	local PASSWORD=$2
	authenticate
	egosh user list -ll | grep \"$USERNAME\" > /dev/null 2>&1
	if [ $? -eq 0 ]
	then
		log "User $USERNAME already exists"
	else
		egosh user add -u $USERNAME -x $PASSWORD 2>&1 | tee -a $LOG_FILE
		local CODE=$?
		if [ $CODE -ne 0 ]
		then
			log "Cannot create user $USERNAME (error code: $CODE)" ERROR
			exit 1
		else
			log "User $USERNAME created successfully" SUCCESS
		fi
	fi
}

createResourceGroupGPU() {
	local RG_GPU_NAME=$1
	getRestUrls
	local CURL_OUT=`curl -s -S -k -w "%{http_code}" -u $EGO_ADMIN_USERNAME:$EGO_ADMIN_PASSWORD -H "Content-Type:application/json" -H "Accept:application/json" -X POST --data '{"name": "'$RG_GPU_NAME'", "type": "Dynamic", "resourcerequirement": "select(ngpus>0)", "slotexpression": "(ngpus)"}' ${EGO_REST_BASE_URL}ego/v1/resourcegroups`
	local RESPONSE=${CURL_OUT:0:(-3)}
	local HTTP_CODE=${CURL_OUT:(-3)}
	if [ "$HTTP_CODE" == "200" ]
	then
		log "Resource group $RG_GPU_NAME created successfully" SUCCESS
	else
		log "Failed to create resource group $RG_GPU_NAME (HTTP CODE $HTTP_CODE), aborting. Output of creation tentative:" ERROR
		log "$RESPONSE" ERROR
		exit 1
	fi
}

getResourceGroupNbHosts() {
	local RG_NAME=$1
	local __resultvar=$2
	authenticate
	local NB_HOSTS=`egosh rg $RG_NAME | grep NHOSTS | awk -F" " '{print $3}'`
	eval $__resultvar="'$NB_HOSTS'"
}

createConsumer() {
	local CONSUMER_PATH=$1
	local EXECUTION_USER=$2
	local RG_CPU_NAME=$3
	local RG_GPU_NAME=$4
	local ADMIN_USER=$5
	authenticate
	if [ "$RG_CPU_NAME" == "$RG_GPU_NAME" ]
	then
		local RG_LIST=$RG_CPU_NAME
	else
		local RG_LIST="$RG_CPU_NAME,$RG_GPU_NAME"
	fi
	egosh consumer add $CONSUMER_PATH -e $EXECUTION_USER -a $ADMIN_USER -g $RG_LIST 2>&1 | tee -a $LOG_FILE
}

installIfix() {
	IFIX_FILE=$1
	TMP_FIX=/tmp/`basename $IFIX_FILE`
	cp -f $IFIX_FILE $TMP_FIX 2>&1 | tee -a $LOG_FILE
	chmod o+r $TMP_FIX 2>&1 | tee -a $LOG_FILE
	su -l $CLUSTERADMIN -c "source $INSTALL_DIR/profile.platform && egoinstallfixes --silent $TMP_FIX" 2>&1 | tee -a $LOG_FILE
	rm -f $TMP_FIX 2>&1 | tee -a $LOG_FILE
}
