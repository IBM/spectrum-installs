#!/bin/sh

#############################
# WARNING: PLEASE READ README.md FIRST
#############################

source `dirname "$(readlink -f "$0")"`/conf/parameters.inc
source `dirname "$(readlink -f "$0")"`/functions/functions.inc
export LOG_FILE=$LOG_DIR/prepare-host_`hostname -s`.log
[[ ! -d $LOG_DIR ]] && mkdir -p $LOG_DIR && chmod 777 $LOG_DIR

log "Starting prepare host script"

[[ ! "$USER" == "root" ]] && log "Current user is not root, aborting" ERROR && exit 1

if ! id -u "$CLUSTERADMIN" >/dev/null 2>&1;
then
	log "Creating $CLUSTERADMIN user"
	useradd $CLUSTERADMIN 2>&1 | tee -a $LOG_FILE
	if [ ${PIPESTATUS[0]} -ne 0 ]
	then
		log "Cannot create user $CLUSTERADMIN, aborting"
		exit 1
	fi
fi

log "Install pre-requisite packages"
yum install -y openssl curl gettext bind-utils net-tools dejavu-serif-fonts ed sudo zip wget bc 2>&1 | tee -a $LOG_FILE

log "Create limits configuration"
LIMITS_SPECTRUM_FILE=/etc/security/limits.d/99-spectrum.conf
if [ ! -f $LIMITS_SPECTRUM_FILE ]
then
	touch $LIMITS_SPECTRUM_FILE 2>&1 | tee -a $LOG_FILE
	echo "root   soft    nproc     65536" >> $LIMITS_SPECTRUM_FILE
	echo "root   hard    nproc     65536" >> $LIMITS_SPECTRUM_FILE
	echo "root   soft    nofile    65536" >> $LIMITS_SPECTRUM_FILE
	echo "root   hard    nofile    65536" >> $LIMITS_SPECTRUM_FILE
	echo "$CLUSTERADMIN   soft    nproc     65536" >> $LIMITS_SPECTRUM_FILE
	echo "$CLUSTERADMIN   hard    nproc     65536" >> $LIMITS_SPECTRUM_FILE
	echo "$CLUSTERADMIN   soft    nofile    65536" >> $LIMITS_SPECTRUM_FILE
	echo "$CLUSTERADMIN   hard    nofile    65536" >> $LIMITS_SPECTRUM_FILE
fi

applyUlimits

log "Prepare host script finished!" SUCCESS
