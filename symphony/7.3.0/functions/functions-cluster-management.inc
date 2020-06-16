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
	while [ $SERVICE_STATE != "STARTED" ]
	do
		if [ $SERVICE_STATE == "ERROR" ]
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

waitForRestUp() {
	log "Wait for EGO Service REST to start"
	waitForEgoServiceStarted REST
	log "Wait for EGO REST URL to be available"
	waitForClientURL REST_BASE_URL_1
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

installIfix() {
	IFIX_FILE=$1
	TMP_FIX=/tmp/`basename $IFIX_FILE`
	cp -f $IFIX_FILE $TMP_FIX 2>&1 | tee -a $LOG_FILE
	chmod o+r $TMP_FIX 2>&1 | tee -a $LOG_FILE
	su -l $CLUSTERADMIN -c "source $INSTALL_DIR/profile.platform && egoinstallfixes --silent $TMP_FIX" 2>&1 | tee -a $LOG_FILE
	rm -f $TMP_FIX 2>&1 | tee -a $LOG_FILE
}

getRestUrl() {
	if [ "$EGO_REST_BASE_URL" == "" ]
	then
		authenticate
		export EGO_REST_BASE_URL=`egosh client view REST_BASE_URL_1 | awk '/DESCRIPTION/ {print $2}'`
	fi
}

deleteResourceGroup() {
	local RG_NAME=$1
	getRestUrl
	local CURL_OUT=`curl -s -S -k -w "%{http_code}" -u $EGO_ADMIN_USERNAME:$EGO_ADMIN_PASSWORD -H "Content-Type:application/json" -H "Accept:application/json" -X DELETE ${EGO_REST_BASE_URL}ego/v1/resourcegroups/$RG_NAME`
	local RESPONSE=${CURL_OUT:0:(-3)}
	local HTTP_CODE=${CURL_OUT:(-3)}
	if [ "$HTTP_CODE" == "204" ]
	then
		log "Resource group $RG_NAME deleted successfully" SUCCESS
	else
		log "Failed to delete resource group $RG_NAME (HTTP CODE $HTTP_CODE), aborting. Output of deletion tentative:" ERROR
		log "$RESPONSE" ERROR
		exit 1
	fi
}

deleteConsumer() {
	local CONSUMER_PATH=$1
	getRestUrl
	local CURL_OUT=`curl -s -S -k -w "%{http_code}" -u $EGO_ADMIN_USERNAME:$EGO_ADMIN_PASSWORD -H "Content-Type:application/json" -H "Accept:application/json" -X DELETE ${EGO_REST_BASE_URL}ego/v1/consumers/$CONSUMER_PATH`
	local RESPONSE=${CURL_OUT:0:(-3)}
	local HTTP_CODE=${CURL_OUT:(-3)}
	if [ "$HTTP_CODE" == "204" ]
	then
		log "Consumer $CONSUMER_PATH deleted successfully" SUCCESS
	else
		log "Failed to delete consumer $CONSUMER_PATH (HTTP CODE $HTTP_CODE), aborting. Output of deletion tentative:" ERROR
		log "$RESPONSE" ERROR
		exit 1
	fi
}

deleteEgoService() {
	local EGO_SERVICE_NAME=$1
	getRestUrl
	local CURL_OUT=`curl -s -S -k -w "%{http_code}" -u $EGO_ADMIN_USERNAME:$EGO_ADMIN_PASSWORD -H "Content-Type:application/json" -H "Accept:application/json" -X DELETE ${EGO_REST_BASE_URL}ego/v1/services/$EGO_SERVICE_NAME`
	local RESPONSE=${CURL_OUT:0:(-3)}
	local HTTP_CODE=${CURL_OUT:(-3)}
	if [ "$HTTP_CODE" == "204" ]
	then
		log "EGO Service $EGO_SERVICE_NAME deleted successfully" SUCCESS
	else
		log "Failed to delete EGO Service $EGO_SERVICE_NAME (HTTP CODE $HTTP_CODE), aborting. Output of deletion tentative:" ERROR
		log "$RESPONSE" ERROR
		exit 1
	fi
}

deleteApplication() {
	local APP_NAME=$1
	authenticate
	soamunreg $APP_NAME -f 2>&1 | tee -a $LOG_FILE
	local CODE=${PIPESTATUS[0]}
	if [ $CODE -ne 0 ]
	then
		log "Failed to delete application $APP_NAME (exit code: $CODE), aborting" ERROR
		exit 1
	fi
}
