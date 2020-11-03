#!/bin/sh

source `dirname "$(readlink -f "$0")"`/conf/parameters.inc
source `dirname "$(readlink -f "$0")"`/conf/lab-environment.inc
source `dirname "$(readlink -f "$0")"`/functions/functions.inc
export LOG_FILE=$LOG_DIR/delete-lab-env_`hostname -s`.log
[[ ! -d $LOG_DIR ]] && mkdir -p $LOG_DIR && chmod 777 $LOG_DIR

log "Starting delete lab environment"

if [ "$1" == "" ]
then
  log "Username of the lab to delete must be passed as argument" ERROR
  exit 1
fi

LAB_USER=$1

getUserExistence $LAB_USER LAB_USER_EXISTS
if [ "$LAB_USER_EXISTS" == "false" ]
then
  log "User $LAB_USER doesn't exist in EGO, aborting" ERROR
  exit 1
fi

log "Deleting lab environment of user $LAB_USER"

log "Wait for cluster to start and SD service to be started"
waitForClusterUp
waitForEgoServiceStarted SD

log "Deleting all applications under consumer /$LAB_USER"
getApplicationsList /$LAB_USER LAB_USER_APPS
for LAB_USER_APP in $LAB_USER_APPS
do
  log "Deleting application under consumer $LAB_USER_APP"
  deleteApplication $LAB_USER_APP
done

log "Deleting all packages under consumer /$LAB_USER"
getConsumersList /$LAB_USER LAB_USER_CONSUMERS
for LAB_USER_CONSUMER in $LAB_USER_CONSUMERS
do
  getPackagesList $LAB_USER_CONSUMER LAB_USER_PACKAGES
  for LAB_USER_PACKAGE in $LAB_USER_PACKAGES
  do
    log "Deleting packge $LAB_USER_PACKAGE in consumer $LAB_USER_CONSUMER"
    deletePackage $LAB_USER_CONSUMER $LAB_USER_PACKAGE
  done
done

log "Deleting consumer /$LAB_USER"
deleteConsumer $LAB_USER

log "Deleting EGO user $LAB_USER"
deleteUser $LAB_USER

if [ "$LAB_CREATE_OS_USER" == "enabled" ]
then
  [[ ! -d $SCRIPTS_TMP_DIR ]] && mkdir -p $SCRIPTS_TMP_DIR
  SCRIPT_DELETE_OS_USER=$SCRIPTS_TMP_DIR/lab-delete-os-user.sh
  echo '#!/bin/sh
if id "'$LAB_USER'" &>/dev/null; then
  userdel -r '$LAB_USER'
  CODE=$?
  if [ $CODE -eq 0 ]; then
    echo "User '$LAB_USER' deleted successfully"
    exit 0
  else
    echo "Failed to delete user '$LAB_USER' (error code: $CODE)"
    exit 1
  fi
fi' > $SCRIPT_DELETE_OS_USER
  chmod +x $SCRIPT_DELETE_OS_USER 2>&1 | tee -a $LOG_FILE

  log "Creating OS user $LAB_USER on master $MASTERHOST"
  runCommandLocalOrRemote $MASTERHOST $SCRIPT_DELETE_OS_USER "false"

  if [[ "$MANAGEMENTHOSTS_FILE" != "" && -f $MANAGEMENTHOSTS_FILE && `wc -l $MANAGEMENTHOSTS_FILE | awk '{print $1}'` -gt 0 ]]
  then
    for MANAGEMENT_HOST in `cat $MANAGEMENTHOSTS_FILE`
    do
      log "Deleting OS user $LAB_USER on management host $MANAGEMENT_HOST"
      runCommandLocalOrRemote $MANAGEMENT_HOST $SCRIPT_DELETE_OS_USER "false"
    done
  fi

  if [[ "$COMPUTEHOSTS_FILE" != "" && -f $COMPUTEHOSTS_FILE && `wc -l $COMPUTEHOSTS_FILE | awk '{print $1}'` -gt 0 ]]
  then
    for COMPUTE_HOST in `cat $COMPUTEHOSTS_FILE`
    do
      log "Deleting OS user $LAB_USER on compute host $COMPUTE_HOST"
      runCommandLocalOrRemote $COMPUTE_HOST $SCRIPT_DELETE_OS_USER "false"
    done
  fi
fi

log "Lab environment of user $LAB_USER deleted successfully!" SUCCESS
