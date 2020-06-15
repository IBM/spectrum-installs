#!/bin/sh

#############################
# WARNING: PLEASE READ README.md FIRST
#############################

source `dirname "$(readlink -f "$0")"`/conf/parameters.inc
source `dirname "$(readlink -f "$0")"`/functions/functions.inc
export LOG_FILE=$LOG_DIR/install-host_`hostname -s`.log
[[ ! -d $LOG_DIR ]] && mkdir -p $LOG_DIR && chown $CLUSTERADMIN:$CLUSTERADMIN $LOG_DIR

log "Starting host installation"

[[ ! "$USER" == "root" ]] && log "Current user is not root, aborting" ERROR && exit 1
[[ -d $BASE_INSTALL_DIR ]] && log "Base install dir $BASE_INSTALL_DIR already exists, aborting" ERROR && exit 1

[[ ! -f $MANAGEMENTHOSTS_FILE ]] && log "File $MANAGEMENTHOSTS_FILE containing list of management hosts doesn't exist, aborting" ERROR && exit 1

[[ ! -f $SYMPHONY_BIN ]] && log "Symphony installer $SYMPHONY_BIN doesn't exist, aborting" ERROR && exit 1
[[ ! -f $SYMPHONY_ENTITLEMENT ]] && log "Symphony entitlement $SYMPHONY_ENTITLEMENT doesn't exist, aborting" ERROR && exit 1

log "Identify the type of current host (master, management or compute)"
determineHostType
log "Current host is $HOST_TYPE"

export IBM_SPECTRUM_SYMPHONY_LICENSE_ACCEPT=Y
export SIMPLIFIEDWEM=N
export DERBY_DB_HOST=$MASTERHOST

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
[[ ! -d $RPMDB_DIR ]] && prepareDir $RPMDB_DIR $CLUSTERADMIN
[[ ! -d $CACHE_DIR ]] && prepareDir $CACHE_DIR $CLUSTERADMIN

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
		exit 2
	fi
else
	log "Installing from RPMs"
	[[ ! -d $INSTALL_FROM_RPMS_TMP_DIR ]] && prepareDir $INSTALL_FROM_RPMS_TMP_DIR $CLUSTERADMIN
	log "Extracting RPMs to $INSTALL_FROM_RPMS_TMP_DIR"
	$SYMPHONY_BIN --extract $INSTALL_FROM_RPMS_TMP_DIR --quiet 2>&1 | tee -a $LOG_FILE
	log "Installing EGO RPMs"
	rpm -ivh --ignoresize --prefix $INSTALL_DIR --dbpath $RPMDB_DIR $INSTALL_FROM_RPMS_TMP_DIR/ego*.rpm 2>&1 | tee -a $LOG_FILE
	RPMINSTALL_ERRORCODE=${PIPESTATUS[0]}
	if [ $RPMINSTALL_ERRORCODE -eq 0 ]
	then
		log "EGO RPMs successfully installed" SUCCESS
	else
		log "Error during installation of EGO RPMs (error code: $PKGINSTALL_ERRORCODE), aborting" ERROR
		exit 2
	fi
	log "Installing SOAM RPMs"
	rpm -ivh --ignoresize --prefix $INSTALL_DIR --dbpath $RPMDB_DIR $INSTALL_FROM_RPMS_TMP_DIR/ascd*.rpm 2>&1 | tee -a $LOG_FILE
	RPMINSTALL_ERRORCODE=${PIPESTATUS[0]}
	if [ $RPMINSTALL_ERRORCODE -eq 0 ]
	then
		log "SOAM RPMs successfully installed" SUCCESS
	else
		log "Error during installation of SOAM RPMs (error code: $PKGINSTALL_ERRORCODE), aborting" ERROR
		exit 2
	fi
	log "Installing NodeJs RPMs"
	rpm -ivh --ignoresize --prefix $INSTALL_DIR --dbpath $RPMDB_DIR $INSTALL_FROM_RPMS_TMP_DIR/nodejs*.rpm 2>&1 | tee -a $LOG_FILE
	RPMINSTALL_ERRORCODE=${PIPESTATUS[0]}
	if [ $RPMINSTALL_ERRORCODE -eq 0 ]
	then
		log "NodeJs RPMs successfully installed" SUCCESS
	else
		log "Error during installation of NodeJs RPMs (error code: $PKGINSTALL_ERRORCODE), aborting" ERROR
		exit 2
	fi
	log "Installing Explorer RPMs"
	rpm -ivh --ignoresize --prefix $INSTALL_DIR --dbpath $RPMDB_DIR $INSTALL_FROM_RPMS_TMP_DIR/explorer*.rpm 2>&1 | tee -a $LOG_FILE
	RPMINSTALL_ERRORCODE=${PIPESTATUS[0]}
	if [ $RPMINSTALL_ERRORCODE -eq 0 ]
	then
		log "Explorer RPMs successfully installed" SUCCESS
	else
		log "Error during installation of Explorer RPMs (error code: $PKGINSTALL_ERRORCODE), aborting" ERROR
		exit 2
	fi
fi

log "Join cluster"
su -l $CLUSTERADMIN -c "source $INSTALL_DIR/profile.platform && egoconfig join $MASTERHOST -f" 2>&1 | tee -a $LOG_FILE

if [ "$HOST_TYPE" == "COMPUTE" ]
then
	log "Installation on this compute host finished!" SUCCESS
	exit 0
fi

if [ "$HOST_TYPE" == "MASTER" ]
then
	log "Entitle Symphony"
	TMP_ENTITLEMENT=/tmp/`basename $SYMPHONY_ENTITLEMENT`
	cp -f $SYMPHONY_ENTITLEMENT $TMP_ENTITLEMENT 2>&1 | tee -a $LOG_FILE
	su -l $CLUSTERADMIN -c "source $INSTALL_DIR/profile.platform && egoconfig setentitlement $TMP_ENTITLEMENT" 2>&1 | tee -a $LOG_FILE
	rm -f $TMP_ENTITLEMENT 2>&1 | tee -a $LOG_FILE
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
source $INSTALL_DIR/profile.platform
egosh ego start 2>&1 | tee -a $LOG_FILE
log "Wait for the cluster to start"
waitForClusterUp

log "Get WEBGUI URL"
waitForGuiUp
WEBGUI_URL=`egosh client view GUIURL_1 | awk '/DESCRIPTION/ {print $2}'`

log "Installation on the master host finished!" SUCCESS
log "You can connect to the web interface: $WEBGUI_URL ($EGO_ADMIN_USERNAME / $EGO_ADMIN_PASSWORD)"
