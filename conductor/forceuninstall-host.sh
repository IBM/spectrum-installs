#!/bin/sh

#############################
# WARNING: PLEASE READ README.md FIRST
#############################

source `dirname "$(readlink -f "$0")"`/conf/parameters.inc
source `dirname "$(readlink -f "$0")"`/functions/functions.inc
export LOG_FILE=$LOG_DIR/forceuninstall-host_`hostname -s`.log
[[ ! -d $LOG_DIR ]] && mkdir -p $LOG_DIR && chmod 777 $LOG_DIR

log "Starting force uninstall host script"

[[ ! "$USER" == "root" ]] && log "Current user is not root, aborting" ERROR && exit 1

[[ ! -f $MANAGEMENTHOSTS_FILE ]] && log "File $MANAGEMENTHOSTS_FILE containing list of management hosts doesn't exist, aborting" ERROR && exit 1

log "Identify the type of current host (master, management or compute)"
determineHostType
log "Current host is $HOST_TYPE"

source $INSTALL_DIR/profile.platform

if [ "$HOST_TYPE" == "MASTER" ]
then
  egosh user logon -u $EGO_ADMIN_USERNAME -x $EGO_ADMIN_PASSWORD >/dev/null 2>&1
  CODE=$?
  if [ $CODE -eq 0 ]
  then
    log "Stop EGO services"
    egosh service stop all 2>&1 | tee -a $LOG_FILE
    log "Wait for EGO services to be stopped"
    waitForEgoServicesStopped
  fi
fi

log "Stopping EGO on current host"
egosh ego shutdown -f 2>&1 | tee -a $LOG_FILE
log "Wait 15 seconds to make sure all EGO processes are stopped"
sleep 15

log "Kill remaining processes on current host"
ps aux | grep "$BASE_INSTALL_DIR" | grep -v grep | awk '{print $2}' | xargs kill -9 > /dev/null 2>&1

log "Deleting ego.sudoers file"
rm -f /etc/ego.sudoers 2>&1 | tee -a $LOG_FILE

grep "source $INSTALL_DIR/profile.platform" $( getent passwd $CLUSTERADMIN | cut -d: -f6 )/.bashrc > /dev/null
if [ $? -eq 0 ]
then
  log "Removing source profile.platform in .bashrc of $CLUSTERADMIN"
  sed -i "s#source $INSTALL_DIR/profile.platform##" $( getent passwd $CLUSTERADMIN | cut -d: -f6 )/.bashrc 2>&1 | tee -a $LOG_FILE
fi

if [ "$INSTALL_TYPE" == "local" ]
then
  log "Deleting base install directory"
  rm -rf $BASE_INSTALL_DIR 2>&1 | tee -a $LOG_FILE
fi

log "Force uninstall host script finished!" SUCCESS
