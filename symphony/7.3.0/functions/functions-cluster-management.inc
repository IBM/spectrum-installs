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

stopEgoServices() {
  authenticate
  log "Stop EGO services"
  egosh service stop all >/dev/null
  log "Wait for EGO services to be stopped"
  waitForEgoServicesStopped
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

waitForEgoServiceStopped() {
  local SERVICE_NAME=$1
  authenticate
  local SERVICE_STATE=`egosh service list -ll | grep \"$SERVICE_NAME\" | awk -F"," '{print $7}' | sed -e 's/\"//g'`
  while [ "$SERVICE_STATE" != "DEFINED" ]
  do
    log "Service $SERVICE_NAME not stopped (state: $SERVICE_STATE) ..."
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

findAvailableUsername() {
  local BASE_USERNAME=$1
  local __resultvar=$2
  local USERID=1
  local USERNAME=$BASE_USERNAME$USERID
  authenticate
  while true
  do
    egosh user view $USERNAME 2>&1 | grep "ACCOUNT" >/dev/null 2>&1
    if [ $? -ne 0 ]
    then
      break
    else
      USERID=$((USERID+1))
      USERNAME=$BASE_USERNAME$USERID
    fi
  done
  eval $__resultvar="'$USERNAME'"
}

deleteUser() {
  local USERNAME=$1
  authenticate
  egosh user delete -u $USERNAME >/dev/null 2>&1
  local CODE=$?
  if [ $CODE -ne 0 ]
  then
    log "Failed to delete user $USERNAME (error code: $CODE)" WARNING
  else
    log "User $USERNAME deleted successfully" SUCCESS
  fi
}

createConsumerSingleRG() {
  local CONSUMER_PATH=$1
  local EXECUTION_USER=$2
  local RG_NAME=$3
  local ADMIN_USER=$4
  authenticate
  egosh consumer add $CONSUMER_PATH -e $EXECUTION_USER -a $ADMIN_USER -g $RG_NAME 2>&1 | tee -a $LOG_FILE
  CODE=${PIPESTATUS[0]}
  if [ $CODE -ne 0 ]
  then
    log "Failed to create consumer $CONSUMER_PATH" WARNING
  fi
}

createConsumer() {
  local CONSUMER_PATH=$1
  local EXECUTION_USER=$2
  local RG_COMPUTE_NAME=$3
  local RG_MANAGEMENT_NAME=$4
  local ADMIN_USER=$5
  authenticate
  egosh consumer add $CONSUMER_PATH -e $EXECUTION_USER -a $ADMIN_USER -g "$RG_COMPUTE_NAME,$RG_MANAGEMENT_NAME" 2>&1 | tee -a $LOG_FILE
  CODE=${PIPESTATUS[0]}
  if [ $CODE -ne 0 ]
  then
    log "Failed to create consumer $CONSUMER_PATH" WARNING
  fi
}

createConsumerStdCfg() {
  local CONSUMER_PATH=$1
  createConsumer $CONSUMER_PATH $CLUSTERADMIN $RG_COMPUTE_NAME $RG_MANAGEMENT_NAME $DEMO_USER
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

assignConsumerRole() {
  local USER=$1
  local ROLE=$2
  local CONSUMER_PATH=$3
  authenticate
  egosh user assignrole -u $USER -r $ROLE -p $CONSUMER_PATH 2>&1 | tee -a $LOG_FILE
  CODE=${PIPESTATUS[0]}
  if [ $CODE -ne 0 ]
  then
    log "Failed to assign role $ROLE to user $USER for consumer $CONSUMER_PATH" WARNING
  fi
}

assignConsumerAdminRole() {
  local ADMIN_USER=$1
  local CONSUMER_PATH=$2
  assignConsumerRole $ADMIN_USER CONSUMER_ADMIN $CONSUMER_PATH
}

assignConsumerUserRole() {
  local ADMIN_USER=$1
  local CONSUMER_PATH=$2
  assignConsumerRole $ADMIN_USER CONSUMER_USER $CONSUMER_PATH
}

createEgoService() {
  local SERVICE_PROFILE=$1
  authenticate
  egosh service add -f $SERVICE_PROFILE 2>&1 | tee -a $LOG_FILE
  CODE=${PIPESTATUS[0]}
  if [ $CODE -ne 0 ]
  then
    log "Failed to create EGO Service using profile $SERVICE_PROFILE" WARNING
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

startEgoService() {
  local EGO_SERVICE_NAME=$1
  authenticate
  egosh service start $EGO_SERVICE_NAME 2>&1 | tee -a $LOG_FILE
  CODE=${PIPESTATUS[0]}
  if [ $CODE -ne 0 ]
  then
    log "Failed to start EGO Service $EGO_SERVICE_NAME" WARNING
  fi
}

stopEgoService() {
  local EGO_SERVICE_NAME=$1
  authenticate
  egosh service stop $EGO_SERVICE_NAME 2>&1 | tee -a $LOG_FILE
  CODE=${PIPESTATUS[0]}
  if [ $CODE -ne 0 ]
  then
    log "Failed to stop EGO Service $EGO_SERVICE_NAME" WARNING
  fi
}

restartEgoService() {
  local EGO_SERVICE_NAME=$1
  stopEgoService $EGO_SERVICE_NAME
  waitForEgoServiceStopped $EGO_SERVICE_NAME
  startEgoService $EGO_SERVICE_NAME
  waitForEgoServiceStarted $EGO_SERVICE_NAME
}

installIfix() {
  IFIX_FILE=$1
  TMP_FIX=/tmp/`basename $IFIX_FILE`
  cp -f $IFIX_FILE $TMP_FIX 2>&1 | tee -a $LOG_FILE
  chmod o+r $TMP_FIX 2>&1 | tee -a $LOG_FILE
  su -l $CLUSTERADMIN -c "source $INSTALL_DIR/profile.platform && egoinstallfixes --silent $TMP_FIX" 2>&1 | tee -a $LOG_FILE
  rm -f $TMP_FIX 2>&1 | tee -a $LOG_FILE
}

installRPMs() {
  local RPM_PATH=$1
  local RPM_NAME=$2
  log "Installing $RPM_NAME RPMs"
  rpm -ivh --ignoresize --prefix $INSTALL_DIR --dbpath $RPMDB_DIR $RPM_PATH 2>&1 | tee -a $LOG_FILE
  RPMINSTALL_ERRORCODE=${PIPESTATUS[0]}
  if [ $RPMINSTALL_ERRORCODE -eq 0 ]
  then
    log "$RPM_NAME RPMs successfully installed" SUCCESS
  else
    log "Error during installation of $RPM_NAME RPMs (error code: $PKGINSTALL_ERRORCODE), aborting" ERROR
    exit 1
  fi
}

applyEntitlement() {
  local ENTITLEMENT_PATH=$1
  local ENTITLEMENT_NAME=$2
  log "Entitle $ENTITLEMENT_NAME"
  TMP_ENTITLEMENT=/tmp/`basename $ENTITLEMENT_PATH`
  cp -f $ENTITLEMENT_PATH $TMP_ENTITLEMENT 2>&1 | tee -a $LOG_FILE
  chmod o+r $TMP_ENTITLEMENT 2>&1 | tee -a $LOG_FILE
  su -l $CLUSTERADMIN -c "source $INSTALL_DIR/profile.platform && egoconfig setentitlement $TMP_ENTITLEMENT" 2>&1 | tee -a $LOG_FILE
  CODE=${PIPESTATUS[0]}
  rm -f $TMP_ENTITLEMENT 2>&1 | tee -a $LOG_FILE
  if [ $CODE -ne 0 ]
  then
    log "Failed to apply $ENTITLEMENT_NAME entitlement" ERROR
    exit 1
  fi
}
