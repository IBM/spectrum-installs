#!/bin/sh

#############################
# WARNING: PLEASE READ README.md FIRST
#############################

source `dirname "$(readlink -f "$0")"`/conf/parameters.inc
source `dirname "$(readlink -f "$0")"`/functions/functions.inc
export LOG_FILE=$LOG_DIR/install-host_`hostname -s`.log
[[ ! -d $LOG_DIR ]] && mkdir -p $LOG_DIR && chmod 777 $LOG_DIR

log "Starting host installation"

[[ ! "$USER" == "root" ]] && log "Current user is not root, aborting" ERROR && exit 1
[[ "$INSTALL_MULTI_HEAD" != "enabled" && -d $BASE_INSTALL_DIR && ! -z "$(ls -A $BASE_INSTALL_DIR)" ]] && log "Base install dir $BASE_INSTALL_DIR already exists and is not empty, aborting" ERROR && exit 1

[[ ! -f $MANAGEMENTHOSTS_FILE ]] && log "File $MANAGEMENTHOSTS_FILE containing list of management hosts doesn't exist, aborting" ERROR && exit 1

[[ ! -f $SYMPHONY_BIN ]] && log "Symphony installer $SYMPHONY_BIN doesn't exist, aborting" ERROR && exit 1
[[ ! -f $SYMPHONY_ENTITLEMENT ]] && log "Symphony entitlement $SYMPHONY_ENTITLEMENT doesn't exist, aborting" ERROR && exit 1

log "Identify the type of current host (master, management or compute)"
determineHostType
log "Current host is $HOST_TYPE"

if [ "$INSTALL_MULTI_HEAD" == "enabled" ]
then
	log "Doing multi-head installation"

	if [ "$HOST_TYPE" == "MASTER" ]
	then
		log "Stopping the cluster"
		stopEgoServices
		log "Stop EGO on all hosts"
		egosh ego shutdown -f all 2>&1 | tee -a $LOG_FILE
		log "Wait $EGO_SHUTDOWN_WAITTIME seconds to make sure all EGO processes are stopped"
		sleep $EGO_SHUTDOWN_WAITTIME
	fi
fi

export IBM_SPECTRUM_SYMPHONY_LICENSE_ACCEPT=Y
export SIMPLIFIEDWEM=N
export DERBY_DB_HOST=$MASTERHOST
if [ "$BASE_PORT" != "" ]
then
	export BASEPORT=$BASE_PORT
fi

if [ "$INSTALL_TYPE" == "shared" ]
then
	log "Doing install on shared filesystem"
	export SHARED_FS_INSTALL=Y
else
	if [ "$HOST_TYPE" == "COMPUTE" ]
	then
		export EGOCOMPUTEHOST=Y
	fi
fi

if [ "$SSL" == "disabled" ]
then
	log "Doing install with SSL disabled"
	export DISABLESSL=Y
else
	log "Doing install with SSL enabled"
fi

log "Creating directories"
[[ ! -d $BASE_INSTALL_DIR ]] && prepareDir $BASE_INSTALL_DIR $CLUSTERADMIN
[[ ! -d $INSTALL_DIR ]] && prepareDir $INSTALL_DIR $CLUSTERADMIN
[[ ! -d $RPMDB_DIR ]] && prepareDir $BASE_INSTALL_DIR $CLUSTERADMIN
[[ ! -d $CACHE_DIR ]] && prepareDir $BASE_INSTALL_DIR $CLUSTERADMIN || changeOwnershipDir $CACHE_DIR $CLUSTERADMIN

log "Install Symphony"
if [ "$INSTALL_FROM_RPMS" == "disabled" ]
then
	$SYMPHONY_BIN --prefix $INSTALL_DIR --dbpath $RPMDB_DIR --quiet 2>&1 | tee -a $LOG_FILE
	PKGINSTALL_ERRORCODE=${PIPESTATUS[0]}
	if [ $PKGINSTALL_ERRORCODE -eq 0 ]
	then
		log "Symphony package successfully installed" SUCCESS
	else
		log "Error during installation of Symphony package (error code: $PKGINSTALL_ERRORCODE), aborting" ERROR
		exit 1
	fi
else
	log "Installing from RPMs"
	[[ ! -d $INSTALL_FROM_RPMS_TMP_DIR ]] && prepareDir $INSTALL_FROM_RPMS_TMP_DIR $CLUSTERADMIN
	log "Extracting RPMs to $INSTALL_FROM_RPMS_TMP_DIR"
	$SYMPHONY_BIN --extract $INSTALL_FROM_RPMS_TMP_DIR --quiet 2>&1 | tee -a $LOG_FILE
	installRPMs "$INSTALL_FROM_RPMS_TMP_DIR/ego*.rpm" EGO
	installRPMs "$INSTALL_FROM_RPMS_TMP_DIR/soam*.rpm" SOAM
	installRPMs "$INSTALL_FROM_RPMS_TMP_DIR/nodejs*.rpm" NodeJs
	installRPMs "$INSTALL_FROM_RPMS_TMP_DIR/explorer*.rpm" Explorer
fi

if [ "$INSTALL_MULTI_HEAD" != "enabled" ]
then
	log "Join cluster"
	su -l $CLUSTERADMIN -c "source $INSTALL_DIR/profile.platform && egoconfig join $MASTERHOST -f" 2>&1 | tee -a $LOG_FILE
fi

if [ "$HOST_TYPE" == "COMPUTE" ]
then
	log "Installation on this compute host finished!" SUCCESS
	exit 0
fi

if [ "$HOST_TYPE" == "MASTER" ]
then
	applyEntitlement $SYMPHONY_ENTITLEMENT Symphony
fi

log "Define settings in ego.conf"
grep "EGO_ENABLE_BORROW_ONLY_CONSUMER=Y" $INSTALL_DIR/kernel/conf/ego.conf > /dev/null
if [ $? != 0 ]
then
	echo "EGO_ENABLE_BORROW_ONLY_CONSUMER=Y" >> $INSTALL_DIR/kernel/conf/ego.conf
fi
grep "EGO_RSH=ssh" $INSTALL_DIR/kernel/conf/ego.conf > /dev/null
if [ $? != 0 ]
then
	echo "EGO_RSH=ssh" >> $INSTALL_DIR/kernel/conf/ego.conf
fi

if [ "$EGO_SHARED_DIR" != "" ]
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

if [ "$IFIX546970_EGOMGMT" != "" ]
then
	log "Installing Ifix 546970"
	installIfix "$IFIX546970_EGOMGMT"
fi

if [ "$HOST_TYPE" == "MANAGEMENT" ]
then
	log "Installation on this management host finished!" SUCCESS
	exit 0
fi

log "Start cluster"
applyUlimits
source $INSTALL_DIR/profile.platform
egosh ego start 2>&1 | tee -a $LOG_FILE
log "Wait for the cluster to start"
waitForClusterUp

if [ "$INSTALL_MULTI_HEAD" == "enabled" -a "$POSTINSTALL_AUTOMATIC_CONFIG" == "enabled" ]
then
	log "Wait for EGO service REST to be up"
	waitForRestUp

	log "Doing post-install configuration"
	createConsumerSingleRG "/ManagementServices/SymphonyManagementServices" $CLUSTERADMIN $RG_MANAGEMENT_NAME "Guest"
	createConsumerSingleRG "/ClusterServices/SymphonyClusterServices" $CLUSTERADMIN $RG_INTERNAL_NAME "Guest"
	createConsumer "/SymTesting" $CLUSTERADMIN $RG_COMPUTE_NAME $RG_MANAGEMENT_NAME "Guest"
	createConsumer "/SymTesting/Symping731" $CLUSTERADMIN $RG_COMPUTE_NAME $RG_MANAGEMENT_NAME "Guest"
	createConsumer "/SymExec" $CLUSTERADMIN $RG_COMPUTE_NAME $RG_MANAGEMENT_NAME "Guest"
	createConsumer "/SymExec/SymExec731" $CLUSTERADMIN $RG_COMPUTE_NAME $RG_MANAGEMENT_NAME "Guest"
	createConsumer "/SampleApplications/SOASamples" $CLUSTERADMIN $RG_COMPUTE_NAME $RG_MANAGEMENT_NAME "Guest"
	createConsumer "/SampleApplications/SOADemo" $CLUSTERADMIN $RG_COMPUTE_NAME $RG_MANAGEMENT_NAME "Guest"
	createEgoService "$INSTALL_DIR/gui/conf/post_install/sd.xml"
	restartEgoService purger
	restartEgoService plc
	restartEgoService SYMREST

	log "Removing post-install files from GUI config directory"
	rm -f $INSTALL_DIR/gui/conf/post_install/*
	restartEgoService WEBGUI
fi

log "Get WEBGUI URL"
waitForGuiUp
WEBGUI_URL=`egosh client view GUIURL_1 | awk '/DESCRIPTION/ {print $2}'`

log "Installation on the master host finished!" SUCCESS
log "You can connect to the web interface: $WEBGUI_URL ($EGO_ADMIN_USERNAME / $EGO_ADMIN_PASSWORD)"
