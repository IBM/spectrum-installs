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
[[ -d $INSTALL_DIR && ! -z "$(ls -A $INSTALL_DIR)" ]] && log "Install dir $INSTALL_DIR already exists and is not empty, aborting" ERROR && exit 1

[[ ! -f $MANAGEMENTHOSTS_FILE ]] && log "File $MANAGEMENTHOSTS_FILE containing list of management hosts doesn't exist, aborting" ERROR && exit 1

[[ ! -f $CONDUCTOR_BIN ]] && log "Conductor installer $CONDUCTOR_BIN doesn't exist, aborting" ERROR && exit 1
[[ ! -f $CONDUCTOR_ENTITLEMENT ]] && log "Conductor entitlement $CONDUCTOR_ENTITLEMENT doesn't exist, aborting" ERROR && exit 1
if [ "$IFIX546962_EGOMGMT" != "" ]
then
	[[ ! -f $IFIX546962_EGOMGMT ]] && log "iFix 546962 installer $IFIX546962_EGOMGMT doesn't exist, aborting" ERROR && exit 1
fi

log "Identify the type of current host (master, management or compute)"
determineHostType
log "Current host is $HOST_TYPE"

export IBM_SPECTRUM_CONDUCTOR_LICENSE_ACCEPT=Y
export DERBY_DB_HOST=$MASTERHOST

if [ "$INSTALL_TYPE" == "shared" ]
then
	log "Doing install on shared filesystem"
	export SHARED_FS_INSTALL=Y
else
	if [ "$DEPLOYMENT_TYPE" == "shared" ]
	then
		log "Doing install on local filesystem with shared deployment"
		export ELASTIC_HARVEST_LOCATION=$ELASTIC_HARVEST_DIR
	else
		log "Doing install on local filesystem with local deployment"
	fi
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
[[ ! -d $BASE_INSTALL_DIR ]] && prepareDir $BASE_INSTALL_DIR $CLUSTERADMIN || changeOwnershipDir $BASE_INSTALL_DIR $CLUSTERADMIN
[[ ! -d $INSTALL_DIR ]] && prepareDir $INSTALL_DIR $CLUSTERADMIN || changeOwnershipDir $INSTALL_DIR $CLUSTERADMIN
[[ ! -d $RPMDB_DIR ]] && prepareDir $RPMDB_DIR $CLUSTERADMIN || changeOwnershipDir $RPMDB_DIR $CLUSTERADMIN
[[ ! -d $IG_DIR ]] && prepareDir $IG_DIR $CLUSTERADMIN || changeOwnershipDir $IG_DIR $CLUSTERADMIN
[[ ! -d $ANACONDA_DIR ]] && prepareDir $ANACONDA_DIR $CLUSTERADMIN || changeOwnershipDir $ANACONDA_DIR $CLUSTERADMIN
[[ ! -d $CACHE_DIR ]] && prepareDir $CACHE_DIR $CLUSTERADMIN || changeOwnershipDir $CACHE_DIR $CLUSTERADMIN
[[ ! -d $BASE_SHARED_DIR ]] && prepareDir $BASE_SHARED_DIR $CLUSTERADMIN || changeOwnershipDir $BASE_SHARED_DIR $CLUSTERADMIN
[[ ! -d $NOTEBOOKS_DIR ]] && prepareDir $NOTEBOOKS_DIR $CLUSTERADMIN || changeOwnershipDir $NOTEBOOKS_DIR $CLUSTERADMIN
[[ ! -d $SPARKHISTORY_DIR ]] && prepareDir $SPARKHISTORY_DIR $CLUSTERADMIN || changeOwnershipDir $SPARKHISTORY_DIR $CLUSTERADMIN
[[ ! -d $SPARKHA_DIR ]] && prepareDir $SPARKHA_DIR $CLUSTERADMIN || changeOwnershipDir $SPARKHA_DIR $CLUSTERADMIN
[[ ! -d $SPARKSHUFFLE_DIR ]] && prepareDir $SPARKSHUFFLE_DIR $CLUSTERADMIN || changeOwnershipDir $SPARKSHUFFLE_DIR $CLUSTERADMIN

log "Install Conductor"
if [ "$INSTALL_FROM_RPMS" == "disabled" ]
then
	$CONDUCTOR_BIN --prefix $INSTALL_DIR --dbpath $RPMDB_DIR --quiet 2>&1 | tee -a $LOG_FILE
	PKGINSTALL_ERRORCODE=${PIPESTATUS[0]}
	if [ $PKGINSTALL_ERRORCODE -eq 0 ]
	then
		log "Conductor package successfully installed" SUCCESS
	else
		log "Error during installation of Conductor package (error code: $PKGINSTALL_ERRORCODE), aborting" ERROR
		exit 1
	fi
else
	log "Installing from RPMs"
	[[ ! -d $INSTALL_FROM_RPMS_TMP_DIR ]] && prepareDir $INSTALL_FROM_RPMS_TMP_DIR $CLUSTERADMIN
	log "Extracting RPMs to $INSTALL_FROM_RPMS_TMP_DIR"
	export IBM_SPECTRUM_CONDUCTOR_LICENSE_ACCEPT=Y
	$CONDUCTOR_BIN --extract $INSTALL_FROM_RPMS_TMP_DIR --quiet 2>&1 | tee -a $LOG_FILE
	installRPMs "$INSTALL_FROM_RPMS_TMP_DIR/ego*.rpm" EGO
	installRPMs "$INSTALL_FROM_RPMS_TMP_DIR/openjdkjre*.rpm" OpenJdkJre
	installRPMs "$INSTALL_FROM_RPMS_TMP_DIR/ascd*.rpm" ASCD
	installRPMs "$INSTALL_FROM_RPMS_TMP_DIR/conductor*.rpm" Conductor
	installRPMs "$INSTALL_FROM_RPMS_TMP_DIR/nodejs*.rpm" NodeJs
	installRPMs "$INSTALL_FROM_RPMS_TMP_DIR/explorer*.rpm" Explorer
fi

log "Join cluster"
su -l $CLUSTERADMIN -c "source $INSTALL_DIR/profile.platform && egoconfig join $MASTERHOST -f" 2>&1 | tee -a $LOG_FILE

log "Define EGO_GPU_AUTOCONFIG setting in ego.conf"
echo "EGO_GPU_AUTOCONFIG=Y" >> $INSTALL_DIR/kernel/conf/ego.conf

if [ "$HOST_TYPE" == "COMPUTE" ]
then
	log "Installation on this compute host finished!" SUCCESS
	exit 0
fi

if [ "$HOST_TYPE" == "MASTER" ]
then
	applyEntitlement $CONDUCTOR_ENTITLEMENT Conductor
fi

log "Define settings in ego.conf"
echo "EGO_ENABLE_BORROW_ONLY_CONSUMER=Y" >> $INSTALL_DIR/kernel/conf/ego.conf
echo "EGO_RSH=ssh" >> $INSTALL_DIR/kernel/conf/ego.conf

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

if [ "$IFIX546962_EGOMGMT" != "" ]
then
	log "Installing Ifix 546962"
	installIfix "$IFIX546962_EGOMGMT"
fi

if [ "$HOST_TYPE" == "MANAGEMENT" ]
then
	log "Installation on this management host finished!" SUCCESS
	exit 0
fi

if [ "$INSTALL_TYPE" == "local" -a "$DEPLOYMENT_TYPE" == "shared" ]
then
	log "Configuring shared deployment in ascd configuration file"
	source $INSTALL_DIR/profile.platform
	echo "" >> $EGO_CONFDIR/../../ascd/conf/ascd.conf
	echo "ASCD_SHARED_FS_DEPLOY=ON" >> $EGO_CONFDIR/../../ascd/conf/ascd.conf

	log "Changing the number of instances of SparkCleanup service to 1"
	sed -i "s#<sc:MaxInstances>.*</sc:MaxInstances>#<sc:MaxInstances>1</sc:MaxInstances>#" $EGO_CONFDIR/../../eservice/esc/conf/services/sparkcleanup_service.xml 2>&1 | tee -a $LOG_FILE
fi

log "Start cluster"
applyUlimits
source $INSTALL_DIR/profile.platform
egosh ego start 2>&1 | tee -a $LOG_FILE
log "Wait for the cluster to start"
waitForClusterUp

log "Wait for EGO and ASCD REST URLs to be accessible"
waitForRestUrlsUp

if [ "$RG_GPU_NAME" != "" ]
then
	log "Create GPU Resource Group $RG_GPU_NAME"
	createResourceGroupGPU $RG_GPU_NAME
fi

if [ "$ANACONDA_DISTRIBUTIONS_ID_TO_DELETE" != "" ]
then
	for ANACONDA_DISTRIBUTION_ID_TO_DELETE in $ANACONDA_DISTRIBUTIONS_ID_TO_DELETE
	do
		log "Remove Anaconda distribution $ANACONDA_DISTRIBUTION_ID_TO_DELETE"
		deleteAnacondaDistribution $ANACONDA_DISTRIBUTION_ID_TO_DELETE
	done
fi

if [ "$ANACONDA_DISTRIBUTION_NAME_TO_ADD" != "" ]
then
	log "Create Anaconda distribution $ANACONDA_DISTRIBUTION_NAME_TO_ADD"
	createAnacondaDistribution $ANACONDA_DISTRIBUTION_NAME_TO_ADD
fi

log "Get WEBGUI URL"
waitForGuiUp
WEBGUI_URL=`egosh client view GUIURL_1 | awk '/DESCRIPTION/ {print $2}'`

log "Installation on the master host finished!" SUCCESS
log "You can connect to the web interface: $WEBGUI_URL ($EGO_ADMIN_USERNAME / $EGO_ADMIN_PASSWORD)"
