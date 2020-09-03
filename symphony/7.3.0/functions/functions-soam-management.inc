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
