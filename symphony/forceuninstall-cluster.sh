#!/bin/sh

#############################
# WARNING: PLEASE READ README.md FIRST
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
    if [[ "$SSH_PORT" != "" && "$SSH_PORT" != "22" ]]
    then
      MANAGEMENTHOSTS_FILE_TMP=true
      log "Creating temporary management hosts file to use with pssh, as SSH port is not the default ($SSH_PORT)"
      MANAGEMENTHOSTS_FILE_PSSH=/tmp/management-hosts-pssh_`date +%s%N | md5sum | head -c8`.txt
      \cp -f $MANAGEMENTHOSTS_FILE $MANAGEMENTHOSTS_FILE_PSSH 2>&1 | tee -a $LOG_FILE
      sed -e 's/$/:'$SSH_PORT'/' -i $MANAGEMENTHOSTS_FILE_PSSH 2>&1 | tee -a $LOG_FILE
    else
      MANAGEMENTHOSTS_FILE_TMP=false
      MANAGEMENTHOSTS_FILE_PSSH=$MANAGEMENTHOSTS_FILE
    fi
    log "Uninstalling management hosts using pssh ($PSSH_NBHOSTS_IN_PARALLEL hosts in parallel)"
    pssh -h $MANAGEMENTHOSTS_FILE_PSSH -p $PSSH_NBHOSTS_IN_PARALLEL -t $PSSH_TIMEOUT "`dirname "$(readlink -f "$0")"`/forceuninstall-host.sh" 2>&1 | tee -a $LOG_FILE
    if [ "$MANAGEMENTHOSTS_FILE_TMP" == "true" ]
    then
      \rm -f $MANAGEMENTHOSTS_FILE_PSSH 2>&1 | tee -a $LOG_FILE
    fi
  else
    log "No management hosts to uninstall in $MANAGEMENTHOSTS_FILE"
  fi
  if [ `wc -l $COMPUTEHOSTS_FILE | awk '{print $1}'` -gt 0 ]
  then
    if [[ "$SSH_PORT" != "" && "$SSH_PORT" != "22" ]]
    then
      COMPUTEHOSTS_FILE_TMP=true
      log "Creating temporary compute hosts file to use with pssh, as SSH port is not the default ($SSH_PORT)"
      COMPUTEHOSTS_FILE_PSSH=/tmp/compute-hosts-pssh_`date +%s%N | md5sum | head -c8`.txt
      \cp -f $COMPUTEHOSTS_FILE $COMPUTEHOSTS_FILE_PSSH 2>&1 | tee -a $LOG_FILE
      sed -e 's/$/:'$SSH_PORT'/' -i $COMPUTEHOSTS_FILE_PSSH 2>&1 | tee -a $LOG_FILE
    else
      COMPUTEHOSTS_FILE_TMP=false
      COMPUTEHOSTS_FILE_PSSH=$COMPUTEHOSTS_FILE
    fi
    log "Uninstalling compute hosts using pssh ($PSSH_NBHOSTS_IN_PARALLEL hosts in parallel)"
    pssh -h $COMPUTEHOSTS_FILE_PSSH -p $PSSH_NBHOSTS_IN_PARALLEL -t $PSSH_TIMEOUT "`dirname "$(readlink -f "$0")"`/forceuninstall-host.sh" 2>&1 | tee -a $LOG_FILE
    if [ "$COMPUTEHOSTS_FILE_TMP" == "true" ]
    then
      \rm -f $COMPUTEHOSTS_FILE_PSSH 2>&1 | tee -a $LOG_FILE
    fi
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
