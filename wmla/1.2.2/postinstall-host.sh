#!/bin/sh

#############################
# WARNING: PLEASE READ README.md FIRST
#############################

source `dirname "$(readlink -f "$0")"`/conf/parameters.inc
source `dirname "$(readlink -f "$0")"`/functions/functions.inc
export LOG_FILE=$LOG_DIR/postinstall_`hostname -s`.log
[[ ! -d $LOG_DIR ]] && mkdir -p $LOG_DIR && chmod 777 $LOG_DIR

log "Starting host post-installation"

[[ ! "$USER" == "root" ]] && log "Current user is not root, aborting" ERROR && exit 1

[[ ! -f $MANAGEMENTHOSTS_FILE ]] && log "File $MANAGEMENTHOSTS_FILE containing list of management hosts doesn't exist, aborting" ERROR && exit 1

log "Identify the type of current host (master, management or compute)"
determineHostType
log "Current host is $HOST_TYPE"

source $INSTALL_DIR/profile.platform

if [ "$HOST_TYPE" == "MANAGEMENT" -a "$INSTALL_TYPE" == "shared" -a "$EGO_SHARED_DIR" != "" ]
then
	if [[ ! -d $EGO_SHARED_DIR ]]
	then
		log "Creating EGO High Availability directory $EGO_SHARED_DIR"
		prepareDir $EGO_SHARED_DIR $CLUSTERADMIN
	fi
	log "Configuring EGO High Availability directory $EGO_SHARED_DIR"
	[[ ! -d $SYNC_DIR ]] && prepareDir $SYNC_DIR $CLUSTERADMIN
	while [ -f $SYNC_DIR/egoconfig-mghost.lock ]
	do
		log "Waiting before running command egoconfig mghost as there is a lock from another host ..."
		sleep $STATUS_CHECK_WAITTIME
	done
	hostname -f > $SYNC_DIR/egoconfig-mghost.lock
	su -l $CLUSTERADMIN -c "source $INSTALL_DIR/profile.platform && egoconfig mghost $EGO_SHARED_DIR -f" 2>&1 | tee -a $LOG_FILE
	rm -f $SYNC_DIR/egoconfig-mghost.lock 2>&1 | tee -a $LOG_FILE
	source $INSTALL_DIR/profile.platform
fi

log "Registering EGO init scripts"
egosetrc.sh 2>&1 | tee -a $LOG_FILE

log "## Configuring EGO sudoers"
EGOSETSUDOERS_ARG=""
if [ -f /etc/ego.sudoers ]
then
  EGOSETSUDOERS_ARG="-f"
fi
egosetsudoers.sh $EGOSETSUDOERS_ARG 2>&1 | tee -a $LOG_FILE

grep "source $INSTALL_DIR/profile.platform" $( getent passwd $CLUSTERADMIN | cut -d: -f6 )/.bashrc > /dev/null
if [ $? != 0 ]
then
  log "Adding source profile.platform in .bashrc of $CLUSTERADMIN"
  echo "source $INSTALL_DIR/profile.platform" >> $( getent passwd $CLUSTERADMIN | cut -d: -f6 )/.bashrc
fi

log "Apply limits"
ulimit -n 65536 2>&1 | tee -a $LOG_FILE
ulimit -u 65536 2>&1 | tee -a $LOG_FILE
su -l $CLUSTERADMIN -c "ulimit -n 65536" 2>&1 | tee -a $LOG_FILE
su -l $CLUSTERADMIN -c "ulimit -u 65536" 2>&1 | tee -a $LOG_FILE

log "Starting EGO"
egosh ego start 2>&1 | tee -a $LOG_FILE

log "Host post-installation finished!" SUCCESS
