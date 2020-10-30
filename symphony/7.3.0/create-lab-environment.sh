#!/bin/sh

source `dirname "$(readlink -f "$0")"`/conf/parameters.inc
source `dirname "$(readlink -f "$0")"`/conf/lab-environment.inc
source `dirname "$(readlink -f "$0")"`/functions/functions.inc
export LOG_FILE=$LOG_DIR/create-lab-env_`hostname -s`.log
[[ ! -d $LOG_DIR ]] && mkdir -p $LOG_DIR && chmod 777 $LOG_DIR

log "Starting create lab environment"

log "Wait for cluster to start and SD service to be started"
waitForClusterUp
waitForEgoServiceStarted SD

log "Finding available user id"
findAvailableUsername $LAB_USER_BASE LAB_USER

if [ "$LAB_CREATE_OS_USER" == "enabled" ]
then
  if id "$LAB_USER" &>/dev/null; then
    log "Cannot create OS user $LAB_USER, it already exists" ERROR
    exit 1
  fi

  log "Creating OS user $LAB_USER"
  useradd $LAB_USER -g $CLUSTERADMIN 2>&1 | tee -a $LOG_FILE
  CODE=${PIPESTATUS[0]}
  if [ $CODE -eq 0 ]
  then
    log "OS user $LAB_USER created successfully" SUCCESS
  else
    log "Failed to create OS user $LAB_USER (exit code: $CODE)" ERROR
    exit 1
  fi

  if [[ "$LAB_EXERCISES_TEMPLATES_DIR" != "" && -d "$LAB_EXERCISES_TEMPLATES_DIR" ]]
  then
    LAB_USER_EXERCISES_DIR=`eval echo "~$LAB_USER"`/exercises
    log "Copying $LAB_EXERCISES_TEMPLATES_DIR content to $LAB_USER_EXERCISES_DIR"
    cp -r $LAB_EXERCISES_TEMPLATES_DIR/* $LAB_USER_EXERCISES_DIR 2>&1 | tee -a $LOG_FILE
    chown -R $LAB_USER:$CLUSTERADMIN $LAB_USER_EXERCISES_DIR 2>&1 | tee -a $LOG_FILE
  fi
fi

log "Creating EGO user $LAB_USER"
createUser $LAB_USER $LAB_PASSWORD

log "Creating consumers"
createConsumer /$LAB_USER $CLUSTERADMIN $RG_COMPUTE_NAME $RG_MANAGEMENT_NAME $LAB_USER
createConsumer /$LAB_USER/$LAB_USER-app1 $CLUSTERADMIN $RG_COMPUTE_NAME $RG_MANAGEMENT_NAME $LAB_USER
createConsumer /$LAB_USER/$LAB_USER-app2 $CLUSTERADMIN $RG_COMPUTE_NAME $RG_MANAGEMENT_NAME $LAB_USER

log "Creating sample applications"
createApplication $DEMO_APP_PROFILE_TEMPLATE $LAB_USER-app1 /$LAB_USER/$LAB_USER-app1 $RG_COMPUTE_NAME $RG_MANAGEMENT_NAME
createApplication $DEMO_APP_PROFILE_TEMPLATE $LAB_USER-app2 /$LAB_USER/$LAB_USER-app2 $RG_COMPUTE_NAME $RG_MANAGEMENT_NAME

log "Checking state of $DEMO_VAR_APP_NAME application"
getApplicationState $DEMO_VAR_APP_NAME DEMO_VAR_APP_STATE
log "Application $DEMO_VAR_APP_NAME is $DEMO_VAR_APP_STATE"
if [ "$DEMO_VAR_APP_STATE" == "disabled" ]
then
  log "Enabling VaR demo application"
  enableApplication $DEMO_VAR_APP_NAME

  log "Updating low water mark of $DEMO_VAR_APP_NAME application to 1.0"
  getApplicationProfile $DEMO_VAR_APP_NAME DEMO_VAR_APP_PROFILE
  sed -i 's/taskLowWaterMark="0.0"/taskLowWaterMark="1.0"/g' $DEMO_VAR_APP_PROFILE 2>&1 | tee -a $LOG_FILE
  updateApplication $DEMO_VAR_APP_PROFILE
fi

log "Assigning Consumer user role to $LAB_USER for parent consumer ${DEMO_VAR_CONSUMER_PATH%/*} of consumer $DEMO_VAR_CONSUMER_PATH in which demo application $DEMO_VAR_APP_NAME is deployed"
assignConsumerUserRole $LAB_USER ${DEMO_VAR_CONSUMER_PATH%/*}


log "Lab environment created successfully! ($LAB_USER / $LAB_PASSWORD)"
