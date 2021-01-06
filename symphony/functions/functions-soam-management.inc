#!/bin/sh

createApplication() {
  local PROFILE_TEMPLATE=$1
  local APP_NAME=$2
  local CONSUMER_PATH=$3
  local RG_COMPUTE_NAME=$4
  local RG_MANAGEMENT_NAME=$5
  local TMP_PROFILE=/tmp/app-profile-`date | md5sum | head -c8`.xml
  local CONSUMER_PATH=${CONSUMER_PATH//\//\\\/}
  authenticate
  cp $PROFILE_TEMPLATE $TMP_PROFILE 2>&1 | tee -a $LOG_FILE
  sed -i 's/##APP_NAME##/'$APP_NAME'/g' $TMP_PROFILE 2>&1 | tee -a $LOG_FILE
  sed -i 's/##CONSUMER_PATH##/'$CONSUMER_PATH'/g' $TMP_PROFILE 2>&1 | tee -a $LOG_FILE
  sed -i 's/##RG_COMPUTE_NAME##/'$RG_COMPUTE_NAME'/g' $TMP_PROFILE 2>&1 | tee -a $LOG_FILE
  sed -i 's/##RG_MANAGEMENT_NAME##/'$RG_MANAGEMENT_NAME'/g' $TMP_PROFILE 2>&1 | tee -a $LOG_FILE
  soamreg $TMP_PROFILE -f 2>&1 | tee -a $LOG_FILE
  local CODE=${PIPESTATUS[0]}
  if [ $CODE -eq 0 ]
  then
    log "Application $APP_NAME created successfully" SUCCESS
  else
    log "Failed to create application $APP_NAME (exit code: $CODE), aborting" ERROR
    exit 1
  fi
  rm -f $TMP_PROFILE
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

enableApplication() {
  local APP_NAME=$1
  authenticate
  soamcontrol app enable $APP_NAME 2>&1 | tee -a $LOG_FILE
  local CODE=${PIPESTATUS[0]}
  if [ $CODE -eq 0 ]
  then
    log "Application $APP_NAME enabled successfully" SUCCESS
  else
    log "Failed to enable application $APP_NAME (exit code: $CODE), aborting" ERROR
    exit 1
  fi
}

getApplicationState() {
  local APP_NAME=$1
  local __resultvar=$2
  authenticate
  soamview app $APP_NAME 2>&1 | grep "enabled" >/dev/null 2>&1
  local CODE=$?
  if [ $CODE -eq 0 ]
  then
    local APP_STATE_L=enabled
  else
    soamview app $APP_NAME 2>&1 | grep "disabled" >/dev/null 2>&1
    local CODE=$?
    if [ $CODE -eq 0 ]
    then
      local APP_STATE_L=disabled
    else
      local APP_STATE_L=unknown
    fi
  fi
  eval $__resultvar="'$APP_STATE_L'"
}

getApplicationProfile() {
  local APP_NAME=$1
  local __resultvar=$2
  local APP_PROFILE_PATH=/tmp/$APP_NAME-`date +%s%N | md5sum | head -c8`.xml
  authenticate
  soamview app $APP_NAME -p > $APP_PROFILE_PATH 2>/dev/null
  local CODE=$?
  if [ $CODE -ne 0 ]
  then
    log "Failed to export application profile of application $APP_NAME (error code: $CODE)" WARNING
  fi
  eval $__resultvar="'$APP_PROFILE_PATH'"
}

getApplicationsList() {
  local CONSUMER_PATH=$1
  local __resultvar=$2
  authenticate
  local APPS_L=`soamview app | grep "$CONSUMER_PATH" | awk '{print $1}'`
  eval $__resultvar="'$APPS_L'"
}

updateApplication() {
  local APP_PROFILE=$1
  authenticate
  soamreg $APP_PROFILE -f 2>&1 | tee -a $LOG_FILE
  local CODE=$?
  if [ $CODE -eq 0 ]
  then
    log "Application updated using application profile $APP_PROFILE" SUCCESS
  else
    log "Failed to update application using application profile $APP_PROFILE (error code: $CODE)" WARNING
  fi
}

getPackagesList() {
  local CONSUMER_PATH=$1
  local __resultvar=$2
  authenticate
  local PACKAGES_L=`soamdeploy view -c $CONSUMER_PATH | sed "1 d" | awk '{print $1}'`
  eval $__resultvar="'$PACKAGES_L'"
}

deletePackage() {
  local CONSUMER_PATH=$1
  local PACKAGE_NAME=$2
  authenticate
  soamdeploy remove $PACKAGE_NAME -c $CONSUMER_PATH -f 2>&1 | tee -a $LOG_FILE
}
