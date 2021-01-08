#!/bin/sh

#############################
# WARNING: PLEASE READ README.md FIRST
#############################

source `dirname "$(readlink -f "$0")"`/conf/parameters.inc
source `dirname "$(readlink -f "$0")"`/functions/functions.inc
export LOG_FILE=$LOG_DIR/install-cluster_`hostname -s`.log
[[ ! -d $LOG_DIR ]] && mkdir -p $LOG_DIR && chmod 777 $LOG_DIR

log "Starting cluster installation"

[[ ! "$USER" == "root" ]] && log "Current user is not root, aborting" ERROR && exit 1

[[ ! -f $MANAGEMENTHOSTS_FILE ]] && log "File $MANAGEMENTHOSTS_FILE containing list of management hosts doesn't exist, aborting" ERROR && exit 1
[[ ! -f $COMPUTEHOSTS_FILE ]] && log "File $COMPUTEHOSTS_FILE containing list of compute hosts doesn't exist, aborting" ERROR && exit 1

log "Preparing install scripts in $SCRIPTS_TMP_DIR"
[[ ! -d $SCRIPTS_TMP_DIR ]] && mkdir -p $SCRIPTS_TMP_DIR
SCRIPT_INSTALL_MASTER=$SCRIPTS_TMP_DIR/install-master-host.sh
SCRIPT_INSTALL_MANAGEMENT=$SCRIPTS_TMP_DIR/install-management-host.sh
SCRIPT_INSTALL_COMPUTE=$SCRIPTS_TMP_DIR/install-compute-host.sh
SCRIPT_RESTART_MASTER=$SCRIPTS_TMP_DIR/restart-master-host.sh
SCRIPT_CONFIGURE_MASTER_CANDIDATES=$SCRIPTS_TMP_DIR/configure-master-candidates.sh
SCRIPT_GET_WEBGUI_URL=$SCRIPTS_TMP_DIR/get-webgui-url.sh

echo "#!/bin/sh" > $SCRIPT_INSTALL_MASTER
chmod +x $SCRIPT_INSTALL_MASTER 2>&1 | tee -a $LOG_FILE
echo "`dirname "$(readlink -f "$0")"`/prepare-host.sh" >> $SCRIPT_INSTALL_MASTER
echo "[[ \$? -ne 0 ]] && echo \"Error during execution of prepare-host.sh, aborting\" && exit 1" >> $SCRIPT_INSTALL_MASTER
echo "`dirname "$(readlink -f "$0")"`/install-host.sh" >> $SCRIPT_INSTALL_MASTER
echo "[[ \$? -ne 0 ]] && echo \"Error during execution of install-host.sh, aborting\" && exit 1" >> $SCRIPT_INSTALL_MASTER
if [ "$SSL" == "enabled" -a "$CLUSTERINSTALL_UPDATE_SSL" == "enabled" ]
then
  echo "`dirname "$(readlink -f "$0")"`/update-ssl-host.sh" >> $SCRIPT_INSTALL_MASTER
  echo "[[ \$? -ne 0 ]] && echo \"Error during execution of update-ssl-host.sh, aborting\" && exit 1" >> $SCRIPT_INSTALL_MASTER
fi
echo "`dirname "$(readlink -f "$0")"`/postinstall-host.sh" >> $SCRIPT_INSTALL_MASTER

echo "#!/bin/sh" > $SCRIPT_INSTALL_MANAGEMENT
chmod +x $SCRIPT_INSTALL_MANAGEMENT 2>&1 | tee -a $LOG_FILE
echo "`dirname "$(readlink -f "$0")"`/prepare-host.sh" >> $SCRIPT_INSTALL_MANAGEMENT
echo "[[ \$? -ne 0 ]] && echo \"Error during execution of prepare-host.sh, aborting\" && exit 1" >> $SCRIPT_INSTALL_MANAGEMENT
if [ "$INSTALL_TYPE" == "local" ]
then
  echo "`dirname "$(readlink -f "$0")"`/install-host.sh" >> $SCRIPT_INSTALL_MANAGEMENT
  echo "[[ \$? -ne 0 ]] && echo \"Error during execution of install-host.sh, aborting\" && exit 1" >> $SCRIPT_INSTALL_MANAGEMENT
  if [ "$SSL" == "enabled" -a "$CLUSTERINSTALL_UPDATE_SSL" == "enabled" ]
  then
    echo "`dirname "$(readlink -f "$0")"`/update-ssl-host.sh" >> $SCRIPT_INSTALL_MANAGEMENT
  fi
fi
echo "`dirname "$(readlink -f "$0")"`/postinstall-host.sh" >> $SCRIPT_INSTALL_MANAGEMENT

echo "#!/bin/sh" > $SCRIPT_INSTALL_COMPUTE
chmod +x $SCRIPT_INSTALL_COMPUTE 2>&1 | tee -a $LOG_FILE
echo "`dirname "$(readlink -f "$0")"`/prepare-host.sh" >> $SCRIPT_INSTALL_COMPUTE
echo "[[ \$? -ne 0 ]] && echo \"Error during execution of prepare-host.sh, aborting\" && exit 1" >> $SCRIPT_INSTALL_COMPUTE
if [ "$INSTALL_TYPE" == "local" ]
then
  echo "`dirname "$(readlink -f "$0")"`/install-host.sh" >> $SCRIPT_INSTALL_COMPUTE
  echo "[[ \$? -ne 0 ]] && echo \"Error during execution of install-host.sh, aborting\" && exit 1" >> $SCRIPT_INSTALL_COMPUTE
  if [ "$SSL" == "enabled" -a "$CLUSTERINSTALL_UPDATE_SSL" == "enabled" ]
  then
    echo "`dirname "$(readlink -f "$0")"`/update-ssl-host.sh" >> $SCRIPT_INSTALL_COMPUTE
  fi
fi
echo "`dirname "$(readlink -f "$0")"`/postinstall-host.sh" >> $SCRIPT_INSTALL_COMPUTE

echo "#!/bin/sh" > $SCRIPT_RESTART_MASTER
chmod +x $SCRIPT_RESTART_MASTER 2>&1 | tee -a $LOG_FILE
echo "source $INSTALL_DIR/profile.platform" >> $SCRIPT_RESTART_MASTER
echo "egosh ego restart -f" >> $SCRIPT_RESTART_MASTER

echo "#!/bin/sh" > $SCRIPT_CONFIGURE_MASTER_CANDIDATES
chmod +x $SCRIPT_CONFIGURE_MASTER_CANDIDATES 2>&1 | tee -a $LOG_FILE
export MASTER_CANDIDATES_NOSPACE=`echo $MASTER_CANDIDATES | sed 's/ //g'`
echo "su -l $CLUSTERADMIN -c 'source $INSTALL_DIR/profile.platform && egoconfig masterlist $MASTER_CANDIDATES_NOSPACE -f'" >> $SCRIPT_CONFIGURE_MASTER_CANDIDATES

echo "#!/bin/sh" > $SCRIPT_GET_WEBGUI_URL
chmod +x $SCRIPT_GET_WEBGUI_URL 2>&1 | tee -a $LOG_FILE
echo "export LOG_FILE=$LOG_DIR/get-webgui-url_\`hostname -s\`.log" >> $SCRIPT_GET_WEBGUI_URL
echo "source `dirname "$(readlink -f "$0")"`/conf/parameters.inc" >> $SCRIPT_GET_WEBGUI_URL
echo "source `dirname "$(readlink -f "$0")"`/functions/functions-common.inc" >> $SCRIPT_GET_WEBGUI_URL
echo "source `dirname "$(readlink -f "$0")"`/functions/functions-cluster-management.inc" >> $SCRIPT_GET_WEBGUI_URL
echo "waitForClusterUp" >> $SCRIPT_GET_WEBGUI_URL
echo "waitForGuiUp" >> $SCRIPT_GET_WEBGUI_URL
echo 'source $INSTALL_DIR/profile.platform' >> $SCRIPT_GET_WEBGUI_URL
echo "WEBGUI_URL=\`egosh client view GUIURL_1 | awk '/DESCRIPTION/ {print \$2}'\`" >> $SCRIPT_GET_WEBGUI_URL
echo 'echo "You can connect to the web interface: $WEBGUI_URL ($EGO_ADMIN_USERNAME / $EGO_ADMIN_PASSWORD)"' >> $SCRIPT_GET_WEBGUI_URL

log "Installing master host $MASTERHOST"
runCommandLocalOrRemote $MASTERHOST $SCRIPT_INSTALL_MASTER "true"

if [ "$PSSH_NBHOSTS_IN_PARALLEL" == "" ]
then
  PSSH_NBHOSTS_IN_PARALLEL=0
fi

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
    log "Installing management hosts using pssh ($PSSH_NBHOSTS_IN_PARALLEL hosts in parallel)"
    pssh -h $MANAGEMENTHOSTS_FILE_PSSH -p $PSSH_NBHOSTS_IN_PARALLEL -t $PSSH_TIMEOUT $SCRIPT_INSTALL_MANAGEMENT 2>&1 | tee -a $LOG_FILE
    if [ "$MANAGEMENTHOSTS_FILE_TMP" == "true" ]
    then
      \rm -f $MANAGEMENTHOSTS_FILE_PSSH 2>&1 | tee -a $LOG_FILE
    fi
    log "Restart EGO on master host to take into account new management hosts"
    runCommandLocalOrRemote $MASTERHOST $SCRIPT_RESTART_MASTER "false"
    log "Wait $EGO_SHUTDOWN_WAITTIME seconds to make sure all EGO processes restarted"
    sleep $EGO_SHUTDOWN_WAITTIME
    if [ "$MASTER_CANDIDATES" != "" ]
    then
      log "Configuring master candidates list"
      runCommandLocalOrRemote $MASTERHOST $SCRIPT_CONFIGURE_MASTER_CANDIDATES "false"
      log "Restart EGO on master host to take into account new master candidates list"
      runCommandLocalOrRemote $MASTERHOST $SCRIPT_RESTART_MASTER "false"
      log "Wait $EGO_SHUTDOWN_WAITTIME seconds to make sure all EGO processes restarted"
      sleep $EGO_SHUTDOWN_WAITTIME
    fi
  else
    log "No management hosts to install in $MANAGEMENTHOSTS_FILE"
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
    log "Installing compute hosts using pssh ($PSSH_NBHOSTS_IN_PARALLEL hosts in parallel)"
    pssh -h $COMPUTEHOSTS_FILE_PSSH -p $PSSH_NBHOSTS_IN_PARALLEL -t $PSSH_TIMEOUT $SCRIPT_INSTALL_COMPUTE 2>&1 | tee -a $LOG_FILE
    if [ "$COMPUTEHOSTS_FILE_TMP" == "true" ]
    then
      \rm -f $COMPUTEHOSTS_FILE_PSSH 2>&1 | tee -a $LOG_FILE
    fi
  else
    log "No compute hosts to install in $COMPUTEHOSTS_FILE"
  fi
else
  if [ `wc -l $MANAGEMENTHOSTS_FILE | awk '{print $1}'` -gt 0 ]
  then
    log "Installing management hosts sequentially"
    for MANAGEMENT_HOST in `cat $MANAGEMENTHOSTS_FILE`
    do
      log "Installing management host $MANAGEMENT_HOST"
      runCommandLocalOrRemote $MANAGEMENT_HOST $SCRIPT_INSTALL_MANAGEMENT "false"
    done
    log "Restart EGO on master host to take into account new management hosts"
    runCommandLocalOrRemote $MASTERHOST $SCRIPT_RESTART_MASTER "false"
    log "Wait $EGO_SHUTDOWN_WAITTIME seconds to make sure all EGO processes restarted"
    sleep $EGO_SHUTDOWN_WAITTIME
    if [ "$MASTER_CANDIDATES" != "" ]
    then
      log "Configuring master candidates list"
      runCommandLocalOrRemote $MASTERHOST $SCRIPT_CONFIGURE_MASTER_CANDIDATES "false"
      log "Restart EGO on master host to take into account new master candidates list"
      runCommandLocalOrRemote $MASTERHOST $SCRIPT_RESTART_MASTER "false"
      log "Wait $EGO_SHUTDOWN_WAITTIME seconds to make sure all EGO processes restarted"
      sleep $EGO_SHUTDOWN_WAITTIME
    fi
  else
    log "No management hosts to install in $MANAGEMENTHOSTS_FILE"
  fi
  if [ `wc -l $COMPUTEHOSTS_FILE | awk '{print $1}'` -gt 0 ]
  then
    log "Installing compute hosts sequentially"
    for COMPUTE_HOST in `cat $COMPUTEHOSTS_FILE`
    do
      log "Installing compute host $COMPUTE_HOST"
      runCommandLocalOrRemote $COMPUTE_HOST $SCRIPT_INSTALL_COMPUTE "false"
    done
  else
    log "No compute hosts to install in $COMPUTEHOSTS_FILE"
  fi
fi

if [ "$CLUSTERINSTALL_CREATE_USER_ENVIRONMENT" == "enabled" ]
then
  if [ `wc -l $MANAGEMENTHOSTS_FILE | awk '{print $1}'` -gt 0 ] || [ `wc -l $COMPUTEHOSTS_FILE | awk '{print $1}'` -gt 0 ]
  then
    log "Wait $CLUSTERINSTALL_WAITTIME_BEFORE_CREATE_USER_ENVIRONMENT seconds for hosts to to be up before creating user environment"
    sleep $CLUSTERINSTALL_WAITTIME_BEFORE_CREATE_USER_ENVIRONMENT
  fi

  log "Creating user environment"
  runCommandLocalOrRemote $MASTERHOST "`dirname "$(readlink -f "$0")"`/create-user-environment.sh" "false"
fi

log "Get WEBGUI URL"
runCommandLocalOrRemote $MASTERHOST $SCRIPT_GET_WEBGUI_URL "false"

log "Cluster Installation finished!" SUCCESS
