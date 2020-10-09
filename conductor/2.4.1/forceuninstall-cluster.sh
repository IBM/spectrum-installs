#!/bin/sh

#############################
# WARNING: THIS SCRIPT WILL STOP CONDUCTOR CLUSTER AND REMOVE ALL DIRECTORIES! USE WITH CAUTION!
#############################

source `dirname "$(readlink -f "$0")"`/conf/parameters.inc
source `dirname "$(readlink -f "$0")"`/functions/functions.inc
export LOG_FILE=$LOG_DIR/forceuninstall-cluster_`hostname -s`.log
[[ ! -d $LOG_DIR ]] && mkdir -p $LOG_DIR && chmod 777 $LOG_DIR

log "Starting force uninstall cluster script"

[[ ! "$USER" == "root" ]] && log "Current user is not root, aborting" ERROR && exit 1

[[ ! -f $MANAGEMENTHOSTS_FILE ]] && log "File $MANAGEMENTHOSTS_FILE containing list of management hosts doesn't exist, aborting" ERROR && exit 1
[[ ! -f $COMPUTEHOSTS_FILE ]] && log "File $COMPUTEHOSTS_FILE containing list of compute hosts doesn't exist, aborting" ERROR && exit 1

[[ ! -d $SCRIPTS_TMP_DIR ]] && prepareDir $SCRIPTS_TMP_DIR $CLUSTERADMIN

SCRIPT_DELETE_DIRECTORIES=$SCRIPTS_TMP_DIR/forceuninstall_delete_directories.sh

echo "#!/bin/sh" > $SCRIPT_DELETE_DIRECTORIES
chmod +x $SCRIPT_DELETE_DIRECTORIES 2>&1 | tee -a $LOG_FILE
echo "source `dirname "$(readlink -f "$0")"`/conf/parameters.inc" >> $SCRIPT_DELETE_DIRECTORIES
echo "if [ -d \"\$BASE_SHARED_DIR\" ]; then rm -rf \$BASE_SHARED_DIR; fi" >> $SCRIPT_DELETE_DIRECTORIES
echo "if [ -d \"\$EGO_SHARED_DIR\" ]; then rm -rf \$EGO_SHARED_DIR; fi" >> $SCRIPT_DELETE_DIRECTORIES
if [ "$INSTALL_TYPE" == "shared" ]
then
  echo "if [ -d \"\$BASE_INSTALL_DIR\" ]; then rm -rf \$BASE_INSTALL_DIR; fi" >> $SCRIPT_DELETE_DIRECTORIES
fi

log "Uninstalling master host $MASTERHOST"
runCommandLocalOrRemote $MASTERHOST "`dirname "$(readlink -f "$0")"`/forceuninstall-host.sh" "false"

which pssh >/dev/null 2>&1
if [ $? -eq 0 -a $PSSH_NBHOSTS_IN_PARALLEL -gt 1 ]
then
  if [ `wc -l $MANAGEMENTHOSTS_FILE | awk '{print $1}'` -gt 0 ]
  then
    log "Uninstalling management hosts using pssh ($PSSH_NBHOSTS_IN_PARALLEL hosts in parallel)"
    pssh -h $MANAGEMENTHOSTS_FILE -p $PSSH_NBHOSTS_IN_PARALLEL -t $PSSH_TIMEOUT "`dirname "$(readlink -f "$0")"`/forceuninstall-host.sh" 2>&1 | tee -a $LOG_FILE
  else
    log "No management hosts to uninstall in $MANAGEMENTHOSTS_FILE"
  fi
  if [ `wc -l $COMPUTEHOSTS_FILE | awk '{print $1}'` -gt 0 ]
  then
    log "Uninstalling compute hosts using pssh ($PSSH_NBHOSTS_IN_PARALLEL hosts in parallel)"
    pssh -h $COMPUTEHOSTS_FILE -p $PSSH_NBHOSTS_IN_PARALLEL -t $PSSH_TIMEOUT "`dirname "$(readlink -f "$0")"`/forceuninstall-host.sh" 2>&1 | tee -a $LOG_FILE
  else
    log "No compute hosts to uninstall in $COMPUTEHOSTS_FILE"
  fi
else
  if [ `wc -l $MANAGEMENTHOSTS_FILE | awk '{print $1}'` -gt 0 ]
  then
    log "Uninstalling management hosts sequentially"
    for MANAGEMENT_HOST in `cat $MANAGEMENTHOSTS_FILE`
    do
      log "Uninstalling management host $MANAGEMENT_HOST"
      runCommandLocalOrRemote $MANAGEMENT_HOST "`dirname "$(readlink -f "$0")"`/forceuninstall-host.sh" "false"
    done
  else
    log "No management hosts to uninstall in $MANAGEMENTHOSTS_FILE"
  fi
  if [ `wc -l $COMPUTEHOSTS_FILE | awk '{print $1}'` -gt 0 ]
  then
    log "Uninstalling compute hosts sequentially"
    for COMPUTE_HOST in `cat $COMPUTEHOSTS_FILE`
    do
      log "Uninstalling compute host $COMPUTE_HOST"
      runCommandLocalOrRemote $COMPUTE_HOST "`dirname "$(readlink -f "$0")"`/forceuninstall-host.sh" "false"
    done
  else
    log "No compute hosts to uninstall in $COMPUTEHOSTS_FILE"
  fi
fi

log "Deleting shared directories"
runCommandLocalOrRemote $MASTERHOST $SCRIPT_DELETE_DIRECTORIES "false"

log "Force uninstall cluster script finished!" SUCCESS
