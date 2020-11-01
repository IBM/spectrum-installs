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

for APP_NAME in $LAB_USER-app1 $LAB_USER-app2 $LAB_USER-PiApp
do
  getApplicationState $APP_NAME APP_STATE
  if [ "$APP_STATE" != "unknown" ]
  then
    log "Deleting application $APP_NAME"
    deleteApplication $APP_NAME
  fi
done

log "Deleting consumer /$LAB_USER"
deleteConsumer $LAB_USER

log "Deleting EGO user $LAB_USER"
deleteUser $LAB_USER

if [ "$LAB_CREATE_OS_USER" == "enabled" ]
then
  if id "$LAB_USER" &>/dev/null; then
    log "Deleting OS user $LAB_USER"
    userdel -r $LAB_USER 2>&1 | tee -a $LOG_FILE
  fi
fi

log "Lab environment of user $LAB_USER deleted successfully!" SUCCESS
