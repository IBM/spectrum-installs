#!/bin/sh

#############################
# THIS SCRIPT IS FOR TESTING PURPOSE ONLY, IT SHOULD ONLY BE USED BY DEVELOPERS OF SCRIPTS
#############################

#############################
# DESCRIPTION
# This script will execute several installation of Symphony and test correct installation.
# Test cases are executed sequentially.
#
# TEST CASES
# Test id # NB MANAGEMENT # NB COMPUTE # INSTALL_TYPE #   SSL    # bin/rpm # install hadoop # demo env # pssh
#    1    #       0       #      0     #    shared    # enabled  #   bin   #       yes      #   yes    #  yes
#    2    #       0       #      1     #    shared    # enabled  #   rpm   #       no       #   no     #  no
#    3    #       1       #      0     #    shared    # disabled #   bin   #       yes      #   no     #  yes
#    4    #       1       #      0     #    local     # disabled #   bin   #       yes      #   no     #  yes
#    5    #       0       #      1     #    local     # enabled  #   rpm   #       no       #   yes    #  no
#
# USAGE
# 1. Pull the repo on shared FS accessible by hosts defined in PARAMETERS section
# 2. Add Symphony installer and entitlement file, and update path in parameters.inc if necessary
# 3. Add ifixes files and update path in parameters.inc if necessary
# 4. Update parameters in PARAMETERS section at the top of this script
# 5. Execute this script on server defined as SERVER1_HOSTNAME
# 6. Check results in TESTS_DIR
#############################

### PARAMETERS

export SERVER1_HOSTNAME=server1.domain.com
export SERVER2_HOSTNAME=server2.domain.com
export USER_ID=egoadmin
export LOCAL_DIR=/opt/ibmtest
export SHARED_DIR=/nfs/ibmtest

export TESTS_DIR=`dirname "$(readlink -f "$0")"`/tests
export CONFIG_BACKUP_DIR=$TESTS_DIR/initial-config
export LOG_FILE=$TESTS_DIR/tests_details_`hostname -s`.log
export TESTS_SUMMARY=$TESTS_DIR/tests_summary_`hostname -s`.log
export TESTS_RESULT=$TESTS_DIR/tests_result_`hostname -s`.log
export CONF_PARAMETERS_FILE=`dirname "$(readlink -f "$0")"`/conf/parameters.inc
export CONF_MANAGEMENTHOSTS_FILE=`dirname "$(readlink -f "$0")"`/conf/management-hosts.txt
export CONF_COMPUTEHOSTS_FILE=`dirname "$(readlink -f "$0")"`/conf/compute-hosts.txt

### FUNCTIONS

log() {
	local COLOR_RED=`tput setaf 1`
	local COLOR_GREEN=`tput setaf 2`
	local COLOR_YELLOW=`tput setaf 3`
	local COLOR_RESET=`tput sgr0`

	if [ "$2" == "NODATE" ]
	then
		echo $1
		echo $1 >> $LOG_FILE
	else
		LOG_TYPE=$2
		if [ "$LOG_TYPE" == "" -o "$LOG_TYPE" == "NODATE" ]
		then
			LOG_TYPE=INFO
		fi

		case $LOG_TYPE in
			SUCCESS) COLOR=$COLOR_GREEN ;;
			WARNING) COLOR=$COLOR_YELLOW ;;
			ERROR) COLOR=$COLOR_RED ;;
			*) COLOR= ;;
		esac
		echo ${COLOR}`date "+%Y-%m-%d %H:%M:%S"` $1${COLOR_RESET}

		echo `date "+%Y-%m-%d %H:%M:%S"` - $LOG_TYPE - $1 >> $LOG_FILE
	fi
}

getDuration() {
	local START="$1"
	local END="$2"
	export DURATION=`date -u -d @$(($(date -d "$END" '+%s') - $(date -d "$START" '+%s'))) '+%kh %Mm %Ss'`
}

backupConfig() {
  log "Backup initial configuration"
	[[ ! -d "$CONFIG_BACKUP_DIR" ]] && mkdir -p $CONFIG_BACKUP_DIR 2>&1 | tee -a $LOG_FILE
  \cp `dirname "$(readlink -f "$0")"`/conf/* $CONFIG_BACKUP_DIR 2>&1 | tee -a $LOG_FILE
}

restoreConfig() {
  log "Restore initial configuration"
  \cp $CONFIG_BACKUP_DIR/* `dirname "$(readlink -f "$0")"`/conf/ 2>&1 | tee -a $LOG_FILE
}

resetEnvVariables(){
	unset EGO_REST_BASE_URL
}

cleanup() {
	local TEST_ID=$1
	local TEST_DIR=$TESTS_DIR/test$TEST_ID
	log "Starting cleanup after test $TEST_ID"
  `dirname "$(readlink -f "$0")"`/forceuninstall-cluster.sh 2>&1 | tee -a $LOG_FILE
  ERRORCODE=${PIPESTATUS[0]}
	\cp `dirname "$(readlink -f "$0")"`/logs/forceuninstall* $TEST_DIR/logs 2>&1 | tee -a $LOG_FILE
	if [ $ERRORCODE -ne 0 ]
	then
    log "Failed to execute forceuninstall-cluster.sh script (error code: $ERRORCODE), aborting" ERROR
    exit 1
  fi
	source `dirname "$(readlink -f "$0")"`/conf/parameters.inc
  for DIR_TO_REMOVE in $BASE_INSTALL_DIR $EGO_SHARED_DIR $LOG_DIR $SCRIPTS_TMP_DIR $SYNC_DIR $INSTALL_FROM_RPMS_TMP_DIR $SSL_TMP_DIR
  do
    log "Deleting directory $DIR_TO_REMOVE"
    rm -rf $DIR_TO_REMOVE 2>&1 | tee -a $LOG_FILE
  done
	restoreConfig
	resetEnvVariables
}

updateParametersGlobal() {
  local MASTERHOST=$1
  local CLUSTERADMIN=$2
  local INSTALL_TYPE=$3
  local BASE_INSTALL_DIR=$4
	local SSL=$5
	local INSTALL_FROM_RPMS=$6
	local REMOVE_HADOOP_COMPONENTS=$7
  local CLUSTERINSTALL_CREATE_DEMO_ENVIRONMENT=$8
  local PSSH_NBHOSTS_IN_PARALLEL=$9
  sed -i "s#export MASTERHOST=.*#export MASTERHOST=$MASTERHOST#" $CONF_PARAMETERS_FILE 2>&1 | tee -a $LOG_FILE
  sed -i "s#export CLUSTERADMIN=.*#export CLUSTERADMIN=$CLUSTERADMIN#" $CONF_PARAMETERS_FILE 2>&1 | tee -a $LOG_FILE
  sed -i "s#export INSTALL_TYPE=.*#export INSTALL_TYPE=$INSTALL_TYPE#" $CONF_PARAMETERS_FILE 2>&1 | tee -a $LOG_FILE
  sed -i "s#export BASE_INSTALL_DIR=.*#export BASE_INSTALL_DIR=$BASE_INSTALL_DIR#" $CONF_PARAMETERS_FILE 2>&1 | tee -a $LOG_FILE
	sed -i "s#export SSL=.*#export SSL=$SSL#" $CONF_PARAMETERS_FILE 2>&1 | tee -a $LOG_FILE
  sed -i "s#export INSTALL_FROM_RPMS=.*#export INSTALL_FROM_RPMS=$INSTALL_FROM_RPMS#" $CONF_PARAMETERS_FILE 2>&1 | tee -a $LOG_FILE
	sed -i "s#export REMOVE_HADOOP_COMPONENTS=.*#export REMOVE_HADOOP_COMPONENTS=$REMOVE_HADOOP_COMPONENTS#" $CONF_PARAMETERS_FILE 2>&1 | tee -a $LOG_FILE
  sed -i "s#export CLUSTERINSTALL_CREATE_DEMO_ENVIRONMENT=.*#export CLUSTERINSTALL_CREATE_DEMO_ENVIRONMENT=$CLUSTERINSTALL_CREATE_DEMO_ENVIRONMENT#" $CONF_PARAMETERS_FILE 2>&1 | tee -a $LOG_FILE
	sed -i "s#export PSSH_NBHOSTS_IN_PARALLEL=.*#export PSSH_NBHOSTS_IN_PARALLEL=$PSSH_NBHOSTS_IN_PARALLEL#" $CONF_PARAMETERS_FILE 2>&1 | tee -a $LOG_FILE
}

updateParametersMasterCandidates() {
	local MASTER_CANDIDATES="$1"
	local EGO_SHARED_DIR=$2
	sed -i "s#export MASTER_CANDIDATES=.*#export MASTER_CANDIDATES=\"$MASTER_CANDIDATES\"#" $CONF_PARAMETERS_FILE 2>&1 | tee -a $LOG_FILE
	sed -i "s#export EGO_SHARED_DIR=.*#export EGO_SHARED_DIR=$EGO_SHARED_DIR#" $CONF_PARAMETERS_FILE 2>&1 | tee -a $LOG_FILE
}

updateManagementHosts() {
	local MANAGEMENTHOSTS_LIST="$1"
	rm -f $CONF_MANAGEMENTHOSTS_FILE 2>&1 | tee -a $LOG_FILE
	for MANAGEMENTHOST in $MANAGEMENTHOSTS_LIST
	do
		echo $MANAGEMENTHOST >> $CONF_MANAGEMENTHOSTS_FILE
	done
}

updateComputeHosts() {
	local COMPUTEHOSTS_LIST="$1"
	rm -f $CONF_COMPUTEHOSTS_FILE 2>&1 | tee -a $LOG_FILE
	for COMPUTEHOST in $COMPUTEHOSTS_LIST
	do
		echo $COMPUTEHOST >> $CONF_COMPUTEHOSTS_FILE
	done
}

testInstall() {
  local TEST_ID=$1
	local SERVERS_LIST="$2"
	local TEST_DIR=$TESTS_DIR/test$TEST_ID
	source `dirname "$(readlink -f "$0")"`/conf/parameters.inc
	[[ -d $INSTALL_DIR && ! -z "$(ls -A $INSTALL_DIR)" ]] && log "Install dir $INSTALL_DIR already exists and is not empty, aborting" ERROR && exit 1
	local DATE_START=`date`
	echo "Test $TEST_ID" >> $TESTS_SUMMARY
  `dirname "$(readlink -f "$0")"`/install-cluster.sh 2>&1 | tee -a $LOG_FILE
  ERRORCODE=${PIPESTATUS[0]}
	local DATE_END=`date`
	getDuration "$DATE_START" "$DATE_END"
  if [ $ERRORCODE -eq 0 ]
  then
    log "Test $TEST_ID installation successfully" SUCCESS
		echo "- Installation: SUCCESS ($DURATION)" >> $TESTS_SUMMARY
		local INSTALL_RESULT=success
  else
    log "Failed to perform test $TEST_ID installation (error code: $ERRORCODE)" ERROR
		echo "- Installation: FAILED ($DURATION)" >> $TESTS_SUMMARY
		local INSTALL_RESULT=failed
  fi
  log "Copy configuration, log and work files into $TEST_DIR"
  [[ ! -d $TEST_DIR ]] && mkdir -p $TEST_DIR
  \cp -r `dirname "$(readlink -f "$0")"`/conf $TEST_DIR 2>&1 | tee -a $LOG_FILE
  \cp -r `dirname "$(readlink -f "$0")"`/logs $TEST_DIR 2>&1 | tee -a $LOG_FILE
  \cp -r $SCRIPTS_TMP_DIR $TEST_DIR 2>&1 | tee -a $LOG_FILE
	if [ -d "$SYNC_DIR" ]
	then
		\cp -r $SYNC_DIR $TEST_DIR 2>&1 | tee -a $LOG_FILE
	fi
	if [ -d "$SSL_TMP_DIR" ]
	then
		\cp -r $SSL_TMP_DIR $TEST_DIR 2>&1 | tee -a $LOG_FILE
	fi
  ls -Ralh $CACHE_DIR > $TEST_DIR/ls-CACHE_DIR.txt
  if [ $ERRORCODE -eq 0 ]
  then
		sleep 180
		checkInstall $TEST_ID "$SERVERS_LIST"
  fi
	cleanup $TEST_ID
	if [ "$INSTALL_RESULT" == "success" -a "$CHECK_RESULT" == "success" ]
	then
		echo "Test $TEST_ID - SUCCESS" >> $TESTS_RESULT
	else
		echo "Test $TEST_ID - FAILED" >> $TESTS_RESULT
	fi
}

checkInstall() {
	local TEST_ID=$1
	local SERVERS_LIST="$2"
	local DATE_START=`date`
	export CHECK_RESULT=failed
  log "Checking install of test $TEST_ID"
	source `dirname "$(readlink -f "$0")"`/conf/parameters.inc
	log "Checking if INSTALL_DIR exists"
	if [[ -d $INSTALL_DIR && ! -z "$(ls -A $INSTALL_DIR)" ]]
	then
		log "INSTALL_DIR $INSTALL_DIR exists and is not empty" SUCCESS
		echo "- Check INSTALL_DIR: SUCCESS" >> $TESTS_SUMMARY
	else
		log "INSTALL_DIR $INSTALL_DIR doesn't exist or is empty" ERROR
		echo "- Check INSTALL_DIR: FAILED ($INSTALL_DIR doesn't exist)" >> $TESTS_SUMMARY
		return
	fi
	if [ "$EGO_SHARED_DIR" != "" ]
	then
		log "Checking if EGO_SHARED_DIR exists"
		if [[ -d $EGO_SHARED_DIR && ! -z "$(ls -A $EGO_SHARED_DIR)" ]]
		then
			log "EGO_SHARED_DIR $EGO_SHARED_DIR exists and is not empty" SUCCESS
			echo "- Check EGO_SHARED_DIR: SUCCESS" >> $TESTS_SUMMARY
		else
			log "EGO_SHARED_DIR $EGO_SHARED_DIR doesn't exist or is empty" ERROR
			echo "- Check EGO_SHARED_DIR: FAILED ($EGO_SHARED_DIR doesn't exist)" >> $TESTS_SUMMARY
			return
		fi
	fi
	log "Sourcing $INSTALL_DIR/profile.platform"
	source $INSTALL_DIR/profile.platform
	log "Trying to logon (egosh user logon -u $EGO_ADMIN_USERNAME -x $EGO_ADMIN_PASSWORD)"
	egosh user logon -u $EGO_ADMIN_USERNAME -x $EGO_ADMIN_PASSWORD >/dev/null 2>&1
	local CODE=$?
	if [ $CODE -eq 0 ]
	then
		log "Authentication successful" SUCCESS
		echo "- Check authentication: SUCCESS" >> $TESTS_SUMMARY
	else
		log "Cannot authenticate on EGO (error code: $CODE)" ERROR
		echo "- Check authentication: FAILED (error code: $CODE)" >> $TESTS_SUMMARY
		return
	fi
	log "Checking hosts"
	local HOSTS_CHECK=success
	for SERVER in $SERVERS_LIST
	do
		egosh resource list -ll | grep \"$SERVER\" | grep \"ok\" >/dev/null 2>&1
		local CODE=$?
		if [ $CODE -ne 0 ]
		then
			HOSTS_CHECK=failed
			break
		fi
	done
	if [ "$HOSTS_CHECK" == "success" ]
	then
		log "All hosts in OK state" SUCCESS
		echo "- Check hosts: SUCCESS" >> $TESTS_SUMMARY
	else
		log "At least 1 host not in the cluster or not in OK state" ERROR
		egosh resource list -ll 2>&1 | tee -a $LOG_FILE
		echo "- Check hosts: FAILED (at least 1 host not in the cluster or not in OK state)" >> $TESTS_SUMMARY
		return
	fi
	log "Checking EGO services"
	local SERVICES_CHECK=success
	local SERVICES_STATES=`egosh service list -ll | sed -e 1d | awk -F"," '{print $7}' | sed -e 's/\"//g'`
	for SERVICE_STATE in $SERVICES_STATES
	do
		if [ "$SERVICE_STATE" != "STARTED" -a "$SERVICE_STATE" != "DEFINED" -a "$SERVICE_STATE" != "ALLOCATING" ]
		then
			SERVICES_CHECK=failed
			break
		fi
	done
	if [ "$SERVICES_CHECK" == "success" ]
	then
		log "All EGO Services in STARTED, DEFINED or ALLOCATING state" SUCCESS
		echo "- Check EGO Services: SUCCESS" >> $TESTS_SUMMARY
	else
		log "At least 1 service is not in STARTED, DEFINED or ALLOCATING state" ERROR
		egosh service list -ll 2>&1 | tee -a $LOG_FILE
		echo "- Check EGO Services: FAILED (at least 1 service not in STARTED, DEFINED or ALLOCATING state)" >> $TESTS_SUMMARY
		return
	fi
	if [ "$CLUSTERINSTALL_CREATE_DEMO_ENVIRONMENT" == "enabled" ]
	then
		log "Checking demo environment"
		soamview app $DEMO_VAR_APP_NAME | grep "enabled" >/dev/null 2>&1
		CODE=$?
		if [ $CODE -eq 0 ]
		then
			log "Demo application $DEMO_VAR_APP_NAME enabled" SUCCESS
			echo "- Check demo application: SUCCESS" >> $TESTS_SUMMARY
		else
			log "Demo application $DEMO_VAR_APP_NAME not enabled" ERROR
			soamview app $DEMO_VAR_APP_NAME -l 2>&1 | tee -a $LOG_FILE
			echo "- Check demo application: FAILED (application not enabled)" >> $TESTS_SUMMARY
			return
		fi
	fi
	local DATE_END=`date`
	getDuration "$DATE_START" "$DATE_END"
	echo "- Complete check: SUCCESS ($DURATION)" >> $TESTS_SUMMARY
	export CHECK_RESULT=success
}

### MAIN
[[ ! -d $TESTS_DIR ]] && mkdir -p $TESTS_DIR
log "Starting test script"
backupConfig
log "Starting test 1"
updateParametersGlobal $SERVER1_HOSTNAME $USER_ID shared $SHARED_DIR enabled disabled enabled enabled 10
testInstall 1 "$SERVER1_HOSTNAME"
log "Starting test 2"
updateParametersGlobal $SERVER1_HOSTNAME $USER_ID shared $SHARED_DIR enabled enabled disabled disabled 0
updateComputeHosts "$SERVER2_HOSTNAME"
testInstall 2 "$SERVER1_HOSTNAME $SERVER2_HOSTNAME"
log "Starting test 3"
updateParametersGlobal $SERVER1_HOSTNAME $USER_ID shared $SHARED_DIR disabled disabled enabled disabled 10
updateParametersMasterCandidates "$SERVER1_HOSTNAME,$SERVER2_HOSTNAME" $SHARED_DIR/ego-share
updateManagementHosts "$SERVER2_HOSTNAME"
testInstall 3 "$SERVER1_HOSTNAME $SERVER2_HOSTNAME"
log "Starting test 4"
updateParametersGlobal $SERVER1_HOSTNAME $USER_ID local $LOCAL_DIR disabled disabled enabled disabled 10
updateParametersMasterCandidates "$SERVER1_HOSTNAME,$SERVER2_HOSTNAME" $SHARED_DIR/ego-share
updateManagementHosts "$SERVER2_HOSTNAME"
testInstall 4 "$SERVER1_HOSTNAME $SERVER2_HOSTNAME"
log "Starting test 5"
updateParametersGlobal $SERVER1_HOSTNAME $USER_ID local $LOCAL_DIR enabled enabled disabled enabled 0
updateComputeHosts "$SERVER2_HOSTNAME"
testInstall 5 "$SERVER1_HOSTNAME $SERVER2_HOSTNAME"
log "End of test script"
