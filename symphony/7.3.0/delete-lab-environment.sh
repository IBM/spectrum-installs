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
log "Deleting lab environment of user $LAB_USER"

log "Wait for cluster to start and SD service to be started"
waitForClusterUp
waitForEgoServiceStarted SD

log "Deleting application $LAB_USER-app1"
deleteApplication $LAB_USER-app1
log "Deleting application $LAB_USER-app2"
deleteApplication $LAB_USER-app2

log "Deleting consumer /$LAB_USER"
deleteConsumer $LAB_USER

log "Deleting user $LAB_USER"
deleteUser $LAB_USER

log "Lab environment of user $LAB_USER deleted successfully!"
