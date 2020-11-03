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
  [[ ! -d $SCRIPTS_TMP_DIR ]] && mkdir -p $SCRIPTS_TMP_DIR
  SCRIPT_CREATE_OS_USER=$SCRIPTS_TMP_DIR/lab-create-os-user.sh
  echo '#!/bin/sh
if id "'$LAB_USER'" &>/dev/null; then
  echo "Cannot create OS user '$LAB_USER', it already exists"
else
  useradd '$LAB_USER' -g '$CLUSTERADMIN'
  CODE=$?
  if [ $CODE -eq 0 ]; then
    echo "User '$LAB_USER' created successfully"
    exit 0
  else
    echo "Failed to create user '$LAB_USER' (error code: $CODE)"
    exit 1
  fi
fi' > $SCRIPT_CREATE_OS_USER
  chmod +x $SCRIPT_CREATE_OS_USER 2>&1 | tee -a $LOG_FILE

  log "Creating OS user $LAB_USER on master $MASTERHOST"
  runCommandLocalOrRemote $MASTERHOST $SCRIPT_CREATE_OS_USER "false"

  if [[ "$MANAGEMENTHOSTS_FILE" != "" && -f $MANAGEMENTHOSTS_FILE && `wc -l $MANAGEMENTHOSTS_FILE | awk '{print $1}'` -gt 0 ]]
  then
    for MANAGEMENT_HOST in `cat $MANAGEMENTHOSTS_FILE`
    do
      log "Creating OS user $LAB_USER on management host $MANAGEMENT_HOST"
      runCommandLocalOrRemote $MANAGEMENT_HOST $SCRIPT_CREATE_OS_USER "false"
    done
  fi

  if [[ "$COMPUTEHOSTS_FILE" != "" && -f $COMPUTEHOSTS_FILE && `wc -l $COMPUTEHOSTS_FILE | awk '{print $1}'` -gt 0 ]]
  then
    for COMPUTE_HOST in `cat $COMPUTEHOSTS_FILE`
    do
      log "Creating OS user $LAB_USER on compute host $COMPUTE_HOST"
      runCommandLocalOrRemote $COMPUTE_HOST $SCRIPT_CREATE_OS_USER "false"
    done
  fi

  LAB_USER_HOME=`eval echo "~$LAB_USER"`

  log "Defining SYM_USER and SYM_PASSWORD environment variables in $LAB_USER_HOME/.bash_profile"
  echo "export SYM_USER=$LAB_USER" >> $LAB_USER_HOME/.bash_profile
  echo "export SYM_PASSWORD=$LAB_PASSWORD" >> $LAB_USER_HOME/.bash_profile

  if [[ "$LAB_EXERCISES_TEMPLATES_DIR" != "" && -d "$LAB_EXERCISES_TEMPLATES_DIR" ]]
  then
    LAB_EXERCISES_TEMPLATES_DIR_NAME=`basename $LAB_EXERCISES_TEMPLATES_DIR`
    LAB_USER_EXERCISES_DIR=$LAB_USER_HOME/$LAB_EXERCISES_TEMPLATES_DIR_NAME

    log "Creating $LAB_USER_EXERCISES_DIR directory"
    mkdir -p $LAB_USER_EXERCISES_DIR 2>&1 | tee -a $LOG_FILE

    log "Copying $LAB_EXERCISES_TEMPLATES_DIR content to $LAB_USER_EXERCISES_DIR"
    cp -r $LAB_EXERCISES_TEMPLATES_DIR/* $LAB_USER_EXERCISES_DIR 2>&1 | tee -a $LOG_FILE

    log "Changing ownership of directory $LAB_USER_EXERCISES_DIR"
    chown -R $LAB_USER:$CLUSTERADMIN $LAB_USER_EXERCISES_DIR 2>&1 | tee -a $LOG_FILE

    log "Replacing LAB_USER in all .xml files"
    for f in $(find $LAB_USER_EXERCISES_DIR -name "*.xml")
    do
      sed -i 's/##LAB_USER##/'$LAB_USER'/g' $f
    done
  fi
fi

log "Creating EGO user $LAB_USER"
createUser $LAB_USER $LAB_PASSWORD

if [ "$LAB_CREATE_OS_USER" == "enabled" ]
then
  LAB_EXEC_USER=$LAB_USER
else
  LAB_EXEC_USER=$CLUSTERADMIN
fi

log "Creating consumers with execution user $LAB_EXEC_USER"
createConsumer /$LAB_USER $LAB_USER $RG_COMPUTE_NAME $RG_MANAGEMENT_NAME $LAB_EXEC_USER
createConsumer /$LAB_USER/$LAB_USER-app1 $LAB_USER $RG_COMPUTE_NAME $RG_MANAGEMENT_NAME $LAB_EXEC_USER
createConsumer /$LAB_USER/$LAB_USER-app2 $LAB_USER $RG_COMPUTE_NAME $RG_MANAGEMENT_NAME $LAB_EXEC_USER

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


log "Lab environment created successfully! ($LAB_USER / $LAB_PASSWORD)" SUCCESS
