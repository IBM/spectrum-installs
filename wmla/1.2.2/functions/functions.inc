#!/bin/sh

##### GENERAL FUNCTIONS

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
		echo ${COLOR}`date "+%Y-%m-%d %H-%M-%S"` $1${COLOR_RESET}

		echo `date "+%Y-%m-%d %H-%M-%S"` - $LOG_TYPE - $1 >> $LOG_FILE
	fi
}

determineHostType() {
	echo $MASTERHOST | grep `hostname -f` > /dev/null 2>&1
	local IS_HOST_MASTER=$?
	grep `hostname -f` $MANAGEMENTHOSTS_FILE > /dev/null 2>&1
	local IS_HOST_MANAGEMENT=$?
	if [ "$IS_HOST_MASTER" == "0" ]
	then
		export HOST_TYPE="MASTER"
	elif [ "$IS_HOST_MANAGEMENT" == "0" ]
	then
		export HOST_TYPE="MANAGEMENT"
	else
		export HOST_TYPE="COMPUTE"
	fi
}

prepareDir() {
	local DIR_PATH=$1
	local OWNER=$2
	mkdir -p $DIR_PATH 2>&1 | tee -a $LOG_FILE
	if [ ${PIPESTATUS[0]} -ne 0 ]
	then
		log "Cannot create directory $DIR_PATH, aborting" ERROR
		exit 1
	fi
	chown $OWNER:$OWNER $DIR_PATH 2>&1 | tee -a $LOG_FILE
	if [ ${PIPESTATUS[0]} -ne 0 ]
	then
		log "Cannot change ownership of directory $DIR_PATH to $OWNER, aborting" ERROR
		exit 1
	fi
}

waitForPortListening() {
	local PORT=$1
	netstat -plnt | grep ":$PORT" | grep "LISTEN" >/dev/null 2>&1
	local CODE=$?
	while [ $CODE -ne 0 ]
	do
		log "Port $PORT not listening ..."
		sleep $STATUS_CHECK_WAITTIME
		netstat -plnt | grep ":$PORT" | grep "LISTEN" >/dev/null 2>&1
		CODE=$?
	done
}

runCommandLocalOrRemote() {
	local HOST_NAME=$1
	local COMMAND=$2
	local ABORT_IF_FAILED=$3 # Specify true or false
	if [ `hostname -f` == $HOST_NAME -o `hostname -s` == $HOST_NAME ]
	then
		log "Running command $COMMAND locally"
		/bin/sh -c "$COMMAND" 2>&1 | tee -a $LOG_FILE
		local CODE=${PIPESTATUS[0]}
		if [ $CODE -ne 0 ]
		then
			if [ "$ABORT_IF_FAILED" == "true" ]
			then
				log "Error during execution of $COMMAND on $HOST_NAME (exit code: $CODE), aborting" ERROR
				exit 1
			else
				log "Error during execution of $COMMAND on $HOST_NAME (exit code: $CODE)" WARNING
			fi
		fi
	else
		log "Running command $COMMAND remotely"
		ssh -t $HOST_NAME -C "$COMMAND" 2>&1 | tee -a $LOG_FILE
		local CODE=${PIPESTATUS[0]}
		if [ $CODE -ne 0 ]
		then
			if [ "$ABORT_IF_FAILED" == "true" ]
			then
				log "Error during execution of $COMMAND on $HOST_NAME (exit code: $CODE), aborting" ERROR
				exit 1
			else
				log "Error during execution of $COMMAND on $HOST_NAME (exit code: $CODE)" WARNING
			fi
		fi
	fi
}

##### CLUSTER MANAGEMENT FUNCTIONS

authenticate() {
	source $INSTALL_DIR/profile.platform
	egosh client whoami >/dev/null 2>&1
	if [ $? -ne 0 ]
	then
		egosh user logon -u $EGO_ADMIN_USERNAME -x $EGO_ADMIN_PASSWORD >/dev/null 2>&1
		local CODE=$?
		if [ $CODE -ne 0 ]
		then
			log "Cannot authenticate on EGO (error code: $CODE)" ERROR
			exit 1
		fi
	fi
}

restartCluster() {
	log "Stop EGO services"
	egosh service stop all >/dev/null
	log "Wait for EGO services to be stopped"
	waitForEgoServicesStopped
	log "Restart EGO"
	egosh ego restart -f all 2>&1 | tee -a $LOG_FILE
	log "Wait $EGO_SHUTDOWN_WAITTIME seconds to make sure all EGO processes restarted"
	sleep $EGO_SHUTDOWN_WAITTIME
	log "Wait for cluster to start"
	waitForClusterUp
}

waitForClusterUp() {
	source $INSTALL_DIR/profile.platform
	egosh user logon -u $EGO_ADMIN_USERNAME -x $EGO_ADMIN_PASSWORD >/dev/null 2>&1
	local CODE=$?
	while [ $CODE -ne 0 ]
	do
		log "Cluster not started ..."
		sleep $STATUS_CHECK_WAITTIME
		egosh user logon -u $EGO_ADMIN_USERNAME -x $EGO_ADMIN_PASSWORD >/dev/null 2>&1
		local CODE=$?
	done
}

waitForEgoServiceStarted() {
	local SERVICE_NAME=$1
	authenticate
	local SERVICE_STATE=`egosh service list -ll | grep \"$SERVICE_NAME\" | awk -F"," '{print $7}' | sed -e 's/\"//g'`
	while [ $SERVICE_STATE != "STARTED" ]
	do
		log "Service $SERVICE_NAME not started (state: $SERVICE_STATE) ..."
		sleep $STATUS_CHECK_WAITTIME
		local SERVICE_STATE=`egosh service list -ll | grep \"$SERVICE_NAME\" | awk -F"," '{print $7}' | sed -e 's/\"//g'`
	done
}

waitForEgoServicesStopped() {
	authenticate
	local SERVICES_STATES=`egosh service list -ll | sed -e 1d | awk -F"," '{print $7}' | sed -e 's/\"//g'`
	for SERVICE_STATE in $SERVICES_STATES
	do
		if [ "$SERVICE_STATE" != "DEFINED" ]
		then
			log "At least 1 service is not stopped ..."
			sleep $STATUS_CHECK_WAITTIME
			local SERVICES_STATES=`egosh service list -ll | sed -e 1d | awk -F"," '{print $7}' | sed -e 's/\"//g'`
			break
		fi
	done
}

waitForClientURL() {
	local CLIENT_NAME=$1
	authenticate
	egosh client view $CLIENT_NAME 2>/dev/null | awk '/DESCRIPTION/ {print $2}' | grep http >/dev/null 2>&1
	local CODE=$?
	while [ $CODE -ne 0 ]
	do
		log "URL for client $CLIENT_NAME not available ..."
		sleep $STATUS_CHECK_WAITTIME
		egosh client view $CLIENT_NAME 2>/dev/null | awk '/DESCRIPTION/ {print $2}' | grep http >/dev/null 2>&1
		local CODE=$?
	done
}

waitForRestUrlsUp(){
	log "Wait for EGO Service REST to start"
	waitForEgoServiceStarted REST
	log "Wait for EGO Service ascd to start"
	waitForEgoServiceStarted ascd
	log "Wait for EGO REST URL to be available"
	waitForClientURL REST_BASE_URL_1
	log "Wait for ASCD REST URL to be available"
	waitForClientURL ASCD_REST_BASE_URL_1
}

waitForGuiUp() {
	log "Wait for EGO Service REST to start"
	waitForEgoServiceStarted REST
	log "Wait for EGO Service WEBGUI to start"
	waitForEgoServiceStarted WEBGUI
	log "Wait for EGO REST URL to be available"
	waitForClientURL REST_BASE_URL_1
	log "Wait for WEBGUI URL to be available"
	waitForClientURL GUIURL_1
}

getRestUrls() {
	if [ "$EGO_REST_BASE_URL" == "" -o "$ASCD_REST_BASE_URL" == "" ]
	then
		authenticate
		export EGO_REST_BASE_URL=`egosh client view REST_BASE_URL_1 | awk '/DESCRIPTION/ {print $2}'`
		export ASCD_REST_BASE_URL=`egosh client view ASCD_REST_BASE_URL_1 | awk '/DESCRIPTION/ {print $2}'`
	fi
}

createUser() {
	local USERNAME=$1
	local PASSWORD=$2
	authenticate
	egosh user list -ll | grep \"$USERNAME\" > /dev/null 2>&1
	if [ $? -eq 0 ]
	then
		log "User $USERNAME already exists"
	else
		egosh user add -u $USERNAME -x $PASSWORD 2>&1 | tee -a $LOG_FILE
		local CODE=$?
		if [ $CODE -ne 0 ]
		then
			log "Cannot create user $USERNAME (error code: $CODE)" ERROR
			exit 1
		else
			log "User $USERNAME created successfully" SUCCESS
		fi
	fi
}

createResourceGroupGPU() {
	local RG_GPU_NAME=$1
	getRestUrls
	local CURL_OUT=`curl -s -S -k -w "%{http_code}" -u $EGO_ADMIN_USERNAME:$EGO_ADMIN_PASSWORD -H "Content-Type:application/json" -H "Accept:application/json" -X POST --data '{"name": "'$RG_GPU_NAME'", "type": "Dynamic", "resourcerequirement": "select(ngpus>0)", "slotexpression": "(ngpus)"}' ${EGO_REST_BASE_URL}ego/v1/resourcegroups`
	local RESPONSE=${CURL_OUT:0:(-3)}
	local HTTP_CODE=${CURL_OUT:(-3)}
	if [ "$HTTP_CODE" == "200" ]
	then
		log "Resource group $RG_GPU_NAME created successfully" SUCCESS
	else
		log "Failed to create resource group $RG_GPU_NAME (HTTP CODE $HTTP_CODE), aborting. Output of creation tentative:" ERROR
		log "$RESPONSE" ERROR
		exit 1
	fi
}

getResourceGroupNbHosts() {
	local RG_NAME=$1
	local __resultvar=$2
	authenticate
	local NB_HOSTS=`egosh rg $RG_NAME | grep NHOSTS | awk -F" " '{print $3}'`
	eval $__resultvar="'$NB_HOSTS'"
}

createConsumer() {
	local CONSUMER_PATH=$1
	local EXECUTION_USER=$2
	local RG_CPU_NAME=$3
	local RG_GPU_NAME=$4
	local ADMIN_USER=$5
	authenticate
	if [ "$RG_CPU_NAME" == "$RG_GPU_NAME" ]
	then
		local RG_LIST=$RG_CPU_NAME
	else
		local RG_LIST="$RG_CPU_NAME,$RG_GPU_NAME"
	fi
	egosh consumer add $CONSUMER_PATH -e $EXECUTION_USER -a $ADMIN_USER -g $RG_LIST 2>&1 | tee -a $LOG_FILE
}

##### IFIXES INSTALLATION FUNCTIONS

installIfix() {
	IFIX_FILE=$1
	TMP_FIX=/tmp/`basename $IFIX_FILE`
	cp -f $IFIX_FILE $TMP_FIX 2>&1 | tee -a $LOG_FILE
	chmod o+r $TMP_FIX 2>&1 | tee -a $LOG_FILE
	su -l $CLUSTERADMIN -c "source $INSTALL_DIR/profile.platform && egoinstallfixes --silent $TMP_FIX" 2>&1 | tee -a $LOG_FILE
	rm -f $TMP_FIX 2>&1 | tee -a $LOG_FILE
}

##### ANACONDA MANAGEMENT FUNCTIONS

downloadAnacondaDistribution() {
	local ANACONDA_DISTRIBUTION_ID=$1
	local ANACONDA_DISTRIBUTION_FILE=$2
	if [ ! -f ${ANACONDA_DISTRIBUTION_FILE} ]
	then
		ANACONDA_DISTRIBUTION_URL="${ANACONDA_DISTRIB_REPO_BASE_URL}${ANACONDA_DISTRIBUTION_ID}.sh"
		log "Downloading Anaconda distribution from $ANACONDA_DISTRIBUTION_URL into $ANACONDA_DISTRIBUTION_FILE"
		wget -O $ANACONDA_DISTRIBUTION_FILE -q $ANACONDA_DISTRIBUTION_URL 2>&1 | tee -a $LOG_FILE
		local CODE=${PIPESTATUS[0]}
		if [ $CODE -eq 0 ]
		then
			log "Anaconda distribution downloaded successfully" SUCCESS
		else
			log "Failed to download Anaconda distribution (wget exit code $CODE), aborting" ERROR
			exit 1
		fi
	fi
}

createLocalCondaChannel() {
	local ANACONDA_DISTRIBUTION_ID=$1
	local ANACONDA_INSTALL_DIR=$2
	local ANACONDA_LOCAL_CHANNEL_DIR=$3
	local ANACONDA_LOCAL_CHANNEL_ARCHIVE=$4
	local ANACONDA_ENV_PROFILES_LIST=$5
	local ANACONDA_DISTRIBUTION_FILE=$CACHE_DIR/${ANACONDA_DISTRIBUTION_ID}.sh
	local ANACONDA_ENV_ID=0
	downloadAnacondaDistribution $ANACONDA_DISTRIBUTION_ID $ANACONDA_DISTRIBUTION_FILE
	if [ -d "$ANACONDA_INSTALL_DIR" ]
	then
		log "Deleting previous install of Anaconda in $ANACONDA_INSTALL_DIR"
		rm -rf $ANACONDA_INSTALL_DIR 2>&1 | tee -a $LOG_FILE
	fi
	log "Installing Anaconda from distribution $ANACONDA_LOCAL_DISTRIBUTION_NAME in directory $ANACONDA_LOCAL_INSTALL_DIR"
	bash $ANACONDA_DISTRIBUTION_FILE -b -p $ANACONDA_INSTALL_DIR 2>&1 | tee -a $LOG_FILE
	if [ ${PIPESTATUS[0]} -ne 0 ]
	then
		log "Failed to install Anaconda from $ANACONDA_LOCAL_DISTRIBUTION_NAME into $ANACONDA_INSTALL_DIR, aborting" ERROR
		exit 1
	fi
	log "Activating Anaconda"
	. $ANACONDA_INSTALL_DIR/etc/profile.d/conda.sh
	export IBM_POWERAI_LICENSE_ACCEPT=yes
	for ANACONDA_ENV_PROFILE in $ANACONDA_ENV_PROFILES_LIST
	do
		ANACONDA_ENV_ID=$((ANACONDA_ENV_ID+1))
		if [ ! -f $ANACONDA_ENV_PROFILE ]
		then
			log "Profile $ANACONDA_ENV_PROFILE doesn't exist, aborting" ERROR
			exit 1
		fi
		TMP_CONDA_ENV_PROFILE=/tmp/condaEnv-profile-env${ANACONDA_ENV_ID}-`date | md5sum | head -c8`.yaml
		cp $ANACONDA_ENV_PROFILE $TMP_CONDA_ENV_PROFILE 2>&1 | tee -a $LOG_FILE
		sed -i 's/##CONDA_ENV_NAME##/env'$ANACONDA_ENV_ID'/g' $TMP_CONDA_ENV_PROFILE 2>&1 | tee -a $LOG_FILE
		local ATTEMPT_NB=1
		while [ $ATTEMPT_NB -le $ANACONDA_LOCAL_CONDA_ENV_CREATION_NB_ATTEMPT ]
		do
			log "Creating conda environment env$ANACONDA_ENV_ID using profile $ANACONDA_ENV_PROFILE (attempt #$ATTEMPT_NB)"
			$ANACONDA_INSTALL_DIR/bin/conda env create -f $TMP_CONDA_ENV_PROFILE 2>&1 | tee -a $LOG_FILE
			CODE=${PIPESTATUS[0]}
			if [ $CODE -eq 0 ]
			then
				log "Conda environment env$ANACONDA_ENV_ID created successfully" SUCCESS
				break
			else
				if [ $ATTEMPT_NB -eq $ANACONDA_LOCAL_CONDA_ENV_CREATION_NB_ATTEMPT ]
				then
					log "Failed to create conda environment env$ANACONDA_ENV_ID from profile $ANACONDA_ENV_PROFILE (exit code: $CODE), aborting" ERROR
					exit 1
				else
					log "Failed to create conda environment env$ANACONDA_ENV_ID from profile $ANACONDA_ENV_PROFILE (exit code: $CODE)" WARNING
					log "Deleting conda environment env$ANACONDA_ENV_ID"
					$ANACONDA_INSTALL_DIR/bin/conda env remove -n env$ANACONDA_ENV_ID 2>&1 | tee -a $LOG_FILE
					log "Waiting $ANACONDA_LOCAL_CONDA_ENV_CREATION_WAITTIME seconds before trying again"
					sleep $ANACONDA_LOCAL_CONDA_ENV_CREATION_WAITTIME
				fi
			fi
			ATTEMPT_NB=$((ATTEMPT_NB+1))
		done
		rm -f $TMP_CONDA_ENV_PROFILE 2>&1 | tee -a $LOG_FILE
	done
	if [ -d "$ANACONDA_LOCAL_CHANNEL_DIR" ]
	then
		log "Deleting previous local conda channel in $ANACONDA_LOCAL_CHANNEL_DIR"
		rm -rf $ANACONDA_LOCAL_CHANNEL_DIR 2>&1 | tee -a $LOG_FILE
	fi
	log "Creating local conda channel directory $ANACONDA_LOCAL_CHANNEL_DIR"
	prepareDir $ANACONDA_LOCAL_CHANNEL_DIR $CLUSTERADMIN
	prepareDir $ANACONDA_LOCAL_CHANNEL_DIR/linux-64 $CLUSTERADMIN
	prepareDir $ANACONDA_LOCAL_CHANNEL_DIR/linux-ppc64le $CLUSTERADMIN
	log "Moving all packages from $ANACONDA_INSTALL_DIR/pkgs/ in the local conda channel"
	case `uname -m` in
		x86_64) ANACONDA_LOCAL_CHANNEL_ARCH_DIR=$ANACONDA_LOCAL_CHANNEL_DIR/linux-64 ;;
		ppc64le) ANACONDA_LOCAL_CHANNEL_ARCH_DIR=$ANACONDA_LOCAL_CHANNEL_DIR/linux-ppc64le ;;
		*) log "Unsupported architecture (`uname -m`), aborting" ERROR && exit 1 ;;
	esac
	mv $ANACONDA_INSTALL_DIR/pkgs/*.bz2 $ANACONDA_LOCAL_CHANNEL_ARCH_DIR 2>&1 | tee -a $LOG_FILE
	log "Deleting conda environments"
	while [ $ANACONDA_ENV_ID -gt 0 ]
	do
		log "Deleting conda environment env$ANACONDA_ENV_ID"
		$ANACONDA_INSTALL_DIR/bin/conda env remove -n env$ANACONDA_ENV_ID 2>&1 | tee -a $LOG_FILE
		ANACONDA_ENV_ID=$((ANACONDA_ENV_ID-1))
	done
	log "Creating the index for the local conda channel"
	$ANACONDA_INSTALL_DIR/bin/conda index $ANACONDA_LOCAL_CHANNEL_DIR 2>&1 | tee -a $LOG_FILE
	CODE=${PIPESTATUS[0]}
	if [ $CODE -ne 0 ]
	then
		log "Failed to create the index of the local conda channel (exit code: $CODE), aborting" ERROR
		exit 1
	fi
	log "Changing ownership of $ANACONDA_LOCAL_CHANNEL_DIR to $CLUSTERADMIN"
	chown -R $CLUSTERADMIN:$CLUSTERADMIN $ANACONDA_LOCAL_CHANNEL_DIR 2>&1 | tee -a $LOG_FILE
	log "Creating the archive $ANACONDA_LOCAL_CHANNEL_ARCHIVE with the local conda channel $ANACONDA_LOCAL_CHANNEL_DIR"
	tar czf $ANACONDA_LOCAL_CHANNEL_ARCHIVE -C $ANACONDA_LOCAL_CHANNEL_DIR . 2>&1 | tee -a $LOG_FILE
	CODE=${PIPESTATUS[0]}
	if [ $CODE -ne 0 ]
	then
		log "Failed to create the archive containing the local conda channel (exit code: $CODE), aborting" WARNING
	fi
	log "Deleting Anaconda installation directory"
	rm -rf $ANACONDA_INSTALL_DIR 2>&1 | tee -a $LOG_FILE
}

prepareLocalCondaChannel() {
	if [ ! -d "$ANACONDA_LOCAL_CHANNEL_DIR" ]
	then
		if [ ! -f "$ANACONDA_LOCAL_CHANNEL_ARCHIVE" ]
		then
			log "Anaconda local channel directory $ANACONDA_LOCAL_CHANNEL_DIR and the archive $ANACONDA_LOCAL_CHANNEL_ARCHIVE doesn't exist, aborting." ERROR
			exit 1
		fi
		log "Anaconda local channel directory doesn't exist, creating it from archive $ANACONDA_LOCAL_CHANNEL_ARCHIVE"
		tar xzf $ANACONDA_LOCAL_CHANNEL_ARCHIVE -C $ANACONDA_LOCAL_CHANNEL_DIR 2>&1 | tee -a $LOG_FILE
		CODE=${PIPESTATUS[0]}
		if [ $CODE -ne 0 ]
		then
			log "Failed to extract local conda channel from archive $ANACONDA_LOCAL_CHANNEL_ARCHIVE in directory $ANACONDA_LOCAL_CHANNEL_DIR (exit code: $CODE), aborting" ERROR
			exit 1
		fi
		log "Changing ownership of $ANACONDA_LOCAL_CHANNEL_DIR to $CLUSTERADMIN"
		chown -R $CLUSTERADMIN:$CLUSTERADMIN $ANACONDA_LOCAL_CHANNEL_DIR 2>&1 | tee -a $LOG_FILE
		log "Anaconda local channel directory $ANACONDA_LOCAL_CHANNEL_DIR prepared successfully" SUCCESS
	fi
}

modifyCondaEnvironmentProfileWithLocalChannel() {
	local CONDA_ENV_PROFILE_TEMPLATE_SOURCE=$1
	local __resultvar=$2
	if [ ! -f "$CONDA_ENV_PROFILE_TEMPLATE_SOURCE" ]
	then
		log "Conda environment profile template $CONDA_ENV_PROFILE_TEMPLATE_SOURCE doesn't exist, aborting." ERROR
		exit 1
	fi
	CONDA_ENV_PROFILE_TEMPLATE_UPDATED=/tmp/condaEnv-profile-localChannel-`date | md5sum | head -c8`.yaml
	head -n 1 $CONDA_ENV_PROFILE_TEMPLATE_SOURCE > $CONDA_ENV_PROFILE_TEMPLATE_UPDATED
	echo "channels:" >> $CONDA_ENV_PROFILE_TEMPLATE_UPDATED
	echo "  - file://$ANACONDA_LOCAL_CHANNEL_DIR" >> $CONDA_ENV_PROFILE_TEMPLATE_UPDATED
	sed -n '/^dependencies\:/,$p' $CONDA_ENV_PROFILE_TEMPLATE_SOURCE >> $CONDA_ENV_PROFILE_TEMPLATE_UPDATED
	eval $__resultvar="'$CONDA_ENV_PROFILE_TEMPLATE_UPDATED'"
}

createAnacondaDistribution() {
	local ANACONDA_DISTRIBUTION_ID=$1
	getRestUrls
	local ANACONDA_DISTRIBUTION_FILE=$CACHE_DIR/${ANACONDA_DISTRIBUTION_ID}.sh
	local ANACONDA_DISTRIBUTION_NAME=`echo $ANACONDA_DISTRIBUTION_ID | sed 's/[\._]/-/g'`
	export ANACONDA_DISTRIBUTION_VERSION=`echo $ANACONDA_DISTRIBUTION_ID | awk -F\- '{print $2}'`
	export ANACONDA_DISTRIBUTION_PYTHONVERSION=`echo $ANACONDA_DISTRIBUTION_ID | sed 's/Anaconda\(.\).*$/\1/'`
	export ANACONDA_DISTRIBUTION_ARCH=`echo $ANACONDA_DISTRIBUTION_ID | awk -F\- 'BEGIN {OFS="-"} { print $3,$4}'`
	downloadAnacondaDistribution $ANACONDA_DISTRIBUTION_ID $ANACONDA_DISTRIBUTION_FILE
	log "Creating Anaconda Distribution $ANACONDA_DISTRIBUTION_NAME using file $ANACONDA_DISTRIBUTION_FILE"
	local CURL_OUT=`curl -s -S -k -w "%{http_code}" -u $EGO_ADMIN_USERNAME:$EGO_ADMIN_PASSWORD -X POST -H 'Content-Type:multipart/form-data' -H 'Accept:application/json' -F "file=@$ANACONDA_DISTRIBUTION_FILE" ${ASCD_REST_BASE_URL}conductor/v1/anaconda/distributions/?name=$ANACONDA_DISTRIBUTION_NAME\&version=$ANACONDA_DISTRIBUTION_VERSION\&pythonversion=$ANACONDA_DISTRIBUTION_PYTHONVERSION\&arch=$ANACONDA_DISTRIBUTION_ARCH`
	local RESPONSE=${CURL_OUT:0:(-3)}
	local HTTP_CODE=${CURL_OUT:(-3)}
	if [ "$HTTP_CODE" == "201" ]
	then
		log "Anaconda distribution $ANACONDA_DISTRIBUTION_NAME created successfully" SUCCESS
	else
		log "Failed to create anaconda distribution $ANACONDA_DISTRIBUTION_NAME (HTTP CODE $HTTP_CODE), aborting. Output of creation tentative:" ERROR
		log "$RESPONSE" ERROR
		exit 1
	fi
}

createAnacondaInstance() {
	local ANACONDA_SOURCE_DISTRIBUTION_ID=$1
	local ANACONDA_INSTANCE_NAME=$2
	local ANACONDA_INSTANCE_DEPLOY_HOME=$3
	local EXECUTION_USER=$4
	local RG_NAME=$5 # Optional, if not specified Anaconda instance will be deployed on all hosts
	getRestUrls
	if [ "$RG_NAME" != "" ]
	then
		log "Create Anaconda instance $ANACONDA_INSTANCE_NAME using distribution $ANACONDA_SOURCE_DISTRIBUTION_ID on hosts of $RG_NAME Resource Group"
		local ANACONDA_INSTANCE_CREATION_OUTPUT=`curl -s -S -k -u $EGO_ADMIN_USERNAME:$EGO_ADMIN_PASSWORD -H "Content-Type:application/json" -H "Accept:application/json" -X POST --data '{ "anacondainstancename": "'$ANACONDA_INSTANCE_NAME'", "sourcedistributionid": "'$ANACONDA_SOURCE_DISTRIBUTION_ID'", "consumerpath": "/", "parameters": { "anaconda_rg_param": "'$RG_NAME'", "deploy_home": "'$ANACONDA_INSTANCE_DEPLOY_HOME'", "execution_user": "'$EXECUTION_USER'" }, "environmentvariables": { "PATH": "$PATH:/usr/bin", "IBM_POWERAI_LICENSE_ACCEPT": "yes" } }' ${ASCD_REST_BASE_URL}conductor/v1/anaconda/instances/`
	else
		log "Create Anaconda instance $ANACONDA_INSTANCE_NAME using distribution $ANACONDA_SOURCE_DISTRIBUTION_ID on all hosts"
		local ANACONDA_INSTANCE_CREATION_OUTPUT=`curl -s -S -k -u $EGO_ADMIN_USERNAME:$EGO_ADMIN_PASSWORD -H "Content-Type:application/json" -H "Accept:application/json" -X POST --data '{ "anacondainstancename": "'$ANACONDA_INSTANCE_NAME'", "sourcedistributionid": "'$ANACONDA_SOURCE_DISTRIBUTION_ID'", "consumerpath": "/", "parameters": { "deploy_home": "'$ANACONDA_INSTANCE_DEPLOY_HOME'", "execution_user": "'$EXECUTION_USER'" }, "environmentvariables": { "PATH": "$PATH:/usr/bin", "IBM_POWERAI_LICENSE_ACCEPT": "yes" } }' ${ASCD_REST_BASE_URL}conductor/v1/anaconda/instances/`
	fi
	export ANACONDA_INSTANCE_UUID=`echo $ANACONDA_INSTANCE_CREATION_OUTPUT | sed -e 's/"//g'`
	if [ ${#ANACONDA_INSTANCE_UUID} -ne 36 ]
	then
		log "Failed to create Anaconda instance, aborting. Output of creaation tentative:" ERROR
		log "$ANACONDA_INSTANCE_CREATION_OUTPUT" ERROR
		exit 1
	fi
	log "Deploying Anaconda instance $ANACONDA_INSTANCE_NAME"
	local CURL_OUT=`curl -s -S -k -w "%{http_code}" -u $EGO_ADMIN_USERNAME:$EGO_ADMIN_PASSWORD -X PUT ${ASCD_REST_BASE_URL}conductor/v1/anaconda/instances/$ANACONDA_INSTANCE_UUID/deploy`
	local RESPONSE=${CURL_OUT:0:(-3)}
	local HTTP_CODE=${CURL_OUT:(-3)}
	if [ "$HTTP_CODE" == "204" ]
	then
		log "Anaconda instance $ANACONDA_INSTANCE_NAME deployment started successfully" SUCCESS
	else
		log "Failed to start deployment of anaconda instance $ANACONDA_INSTANCE_NAME (HTTP CODE $HTTP_CODE), aborting. Output of deployment tentative:" ERROR
		log "$RESPONSE" ERROR
		exit 1
	fi
	getAnacondaInstanceState $ANACONDA_INSTANCE_UUID
	while [ "$ANACONDA_INSTANCE_STATE" != "READY" ]
	do
		if [ "$ANACONDA_INSTANCE_STATE" == "DEPLOY_ERROR" ]
		then
			log "Anaconda instance deployment failed (Anaconda instance state: $ANACONDA_INSTANCE_STATE)" ERROR
			exit 1
		fi
		log "Anaconda instance deploying (state: $ANACONDA_INSTANCE_STATE) ..."
		sleep $STATUS_CHECK_WAITTIME
		getAnacondaInstanceState $ANACONDA_INSTANCE_UUID
	done
	log "Anaconda instance deployed successfully" SUCCESS
}

createCondaEnvironmentAndWait() {
	local ANACONDA_INSTANCE_UUID=$1
	local CONDA_ENV_PROFILE_TEMPLATE=$2
	local CONDA_ENV_NAME=$3
	local ATTEMPT_NB=1
	while [ $ATTEMPT_NB -le $CONDA_ENV_CREATION_NB_ATTEMPT ]
	do
		log "Creating conda environment $CONDA_ENV_NAME from profile template $CONDA_ENV_PROFILE_TEMPLATE (attempt #$ATTEMPT_NB)"
		createCondaEnvironment $ANACONDA_INSTANCE_UUID $CONDA_ENV_PROFILE_TEMPLATE $CONDA_ENV_NAME
		waitForCondaEnvironmentDeployed $ANACONDA_INSTANCE_UUID $CONDA_ENV_NAME
		if [ $CONDA_ENV_EXITSTATUS -eq 0 ]
		then
			break
		else
			if [ $ATTEMPT_NB -eq $CONDA_ENV_CREATION_NB_ATTEMPT ]
			then
				log "Conda environment $CONDA_ENV_NAME deployment failed $ATTEMPT_NB times (last exit status: $CONDA_ENV_EXITSTATUS), aborting." ERROR
				exit 1
			else
				log "Deleting conda environment $CONDA_ENV_NAME"
				removeCondaEnvironment $ANACONDA_INSTANCE_UUID $CONDA_ENV_NAME
				log "Waiting $CONDA_ENV_CREATION_WAITTIME seconds before trying again"
				sleep $CONDA_ENV_CREATION_WAITTIME
			fi
		fi
		ATTEMPT_NB=$((ATTEMPT_NB+1))
	done
}

createCondaEnvironment() {
	local ANACONDA_INSTANCE_UUID=$1
	local CONDA_ENV_PROFILE_TEMPLATE=$2
	local CONDA_ENV_NAME=$3
	local TMP_CONDA_ENV_PROFILE=/tmp/condaEnv-profile-`date | md5sum | head -c8`.yaml
	getRestUrls
	cp $CONDA_ENV_PROFILE_TEMPLATE $TMP_CONDA_ENV_PROFILE 2>&1 | tee -a $LOG_FILE
	sed -i 's/##CONDA_ENV_NAME##/'$CONDA_ENV_NAME'/g' $TMP_CONDA_ENV_PROFILE 2>&1 | tee -a $LOG_FILE
	local CURL_OUT=`curl -s -S -k -w "%{http_code}" -u $EGO_ADMIN_USERNAME:$EGO_ADMIN_PASSWORD -X POST -H 'Content-Type:multipart/form-data' -H 'Accept:application/json' -F "file=@$TMP_CONDA_ENV_PROFILE" ${ASCD_REST_BASE_URL}conductor/v1/anaconda/instances/$ANACONDA_INSTANCE_UUID/environments`
	local RESPONSE=${CURL_OUT:0:(-3)}
	local HTTP_CODE=${CURL_OUT:(-3)}
	if [ "$HTTP_CODE" == "201" ]
	then
		log "Conda environment $CONDA_ENV_NAME created successfully" SUCCESS
	else
		log "Failed to create conda environment $CONDA_ENV_NAME (HTTP CODE $HTTP_CODE), aborting. Output of creation tentative:" ERROR
		log "$RESPONSE" ERROR
		exit 1
	fi
	rm -f $TMP_CONDA_ENV_PROFILE 2>&1 | tee -a $LOG_FILE
}

waitForCondaEnvironmentDeployed() {
	local ANACONDA_INSTANCE_UUID=$1
	local CONDA_ENV_NAME=$2
	getRestUrls
	log "Waiting for conda environment deployment to start"
	local CONDA_ENV_DEPLOYMENT_STARTED=`curl -s -S -k -u $EGO_ADMIN_USERNAME:$EGO_ADMIN_PASSWORD -H "Accept:application/json" -X GET ${ASCD_REST_BASE_URL}conductor/v1/anaconda/instances/$ANACONDA_INSTANCE_UUID/environments/$CONDA_ENV_NAME |$PYTHON_BIN -c $'import sys, json\nd=json.load(sys.stdin)["operations"][0]["activityinfos"][0]\nif "state" in d:\n  print("true")\nelse:\n  print("false")'`
	while [ "$CONDA_ENV_DEPLOYMENT_STARTED" != "true" ]
	do
		log "Conda environment deployment not started ..."
		sleep $STATUS_CHECK_WAITTIME
		local CONDA_ENV_DEPLOYMENT_STARTED=`curl -s -S -k -u $EGO_ADMIN_USERNAME:$EGO_ADMIN_PASSWORD -H "Accept:application/json" -X GET ${ASCD_REST_BASE_URL}conductor/v1/anaconda/instances/$ANACONDA_INSTANCE_UUID/environments/$CONDA_ENV_NAME |$PYTHON_BIN -c $'import sys, json\nd=json.load(sys.stdin)["operations"][0]["activityinfos"][0]\nif "state" in d:\n  print("true")\nelse:\n  print("false")'`
	done
	getCondaEnvState $ANACONDA_INSTANCE_UUID $CONDA_ENV_NAME
	while [ "$CONDA_ENV_DEPLOYED" != "true" ]
	do
		log "Conda environment deploying ..."
		sleep $STATUS_CHECK_WAITTIME
		getCondaEnvState $ANACONDA_INSTANCE_UUID $CONDA_ENV_NAME
	done
	getCondaEnvExitStatus $ANACONDA_INSTANCE_UUID $CONDA_ENV_NAME
	if [ $CONDA_ENV_EXITSTATUS -eq 0 ]
	then
		log "Conda environment $CONDA_ENV_NAME deployed successfully" SUCCESS
	else
		log "Conda environment $CONDA_ENV_NAME deployment failed (exit status: $CONDA_ENV_EXITSTATUS)" WARNING
	fi
}

removeCondaEnvironment() {
	local ANACONDA_INSTANCE_UUID=$1
	local CONDA_ENV_NAME=$2
	getRestUrls
	local CURL_OUT=`curl -s -S -k -w "%{http_code}" -u $EGO_ADMIN_USERNAME:$EGO_ADMIN_PASSWORD -X DELETE -H 'Accept:application/json' ${ASCD_REST_BASE_URL}conductor/v1/anaconda/instances/$ANACONDA_INSTANCE_UUID/environments/$CONDA_ENV_NAME`
	local RESPONSE=${CURL_OUT:0:(-3)}
	local HTTP_CODE=${CURL_OUT:(-3)}
	if [ "$HTTP_CODE" == "204" ]
	then
		log "Conda environment $CONDA_ENV_NAME delete operation started successfully"
	else
		log "Failed to start delete operation of conda environment $CONDA_ENV_NAME (HTTP CODE $HTTP_CODE), aborting. Output of deletion tentative:" ERROR
		log "$RESPONSE" ERROR
		exit 1
	fi
	log "Waiting for conda environment to be deleted"
	getCondaEnvList $ANACONDA_INSTANCE_UUID
	echo "$CONDA_ENV_LIST" | grep \"$CONDA_ENV_NAME\" > /dev/null 2>&1
	local CODE=$?
	while [ $CODE -eq 0 ]
	do
		log "Conda environment being deleted ..."
		sleep $STATUS_CHECK_WAITTIME
		getCondaEnvList $ANACONDA_INSTANCE_UUID
		echo "$CONDA_ENV_LIST" | grep \"$CONDA_ENV_NAME\" > /dev/null 2>&1
		local CODE=$?
	done
	log "Conda environment deleted successfully"
}

getAnacondaInstanceUUID() {
	local ANACONDA_INSTANCE_NAME=$1
	getRestUrls
	export ANACONDA_INSTANCE_UUID=`curl -s -S -k -u $EGO_ADMIN_USERNAME:$EGO_ADMIN_PASSWORD -H "Accept:application/json" -X GET ${ASCD_REST_BASE_URL}conductor/v1/anaconda/instances | $PYTHON_BIN -c $'import sys,json\nais=json.load(sys.stdin)\nfor ai in ais:\n  if (ai["name"]=="'$ANACONDA_INSTANCE_NAME'"): print ai["id"]["uuid"]'`
}

getAnacondaInstanceState() {
	local ANACONDA_INSTANCE_UUID=$1
	getRestUrls
	export ANACONDA_INSTANCE_STATE=`curl -s -S -k -u $EGO_ADMIN_USERNAME:$EGO_ADMIN_PASSWORD -H "Accept:application/json" -X GET ${ASCD_REST_BASE_URL}conductor/v1/anaconda/instances/$ANACONDA_INSTANCE_UUID |$PYTHON_BIN -c 'import sys, json; print json.load(sys.stdin)["state"]'`
}

getCondaEnvState() {
	local ANACONDA_INSTANCE_UUID=$1
	local CONDA_ENV_NAME=$2
	getRestUrls
	export CONDA_ENV_DEPLOYED=`curl -s -S -k -u $EGO_ADMIN_USERNAME:$EGO_ADMIN_PASSWORD -H "Accept:application/json" -X GET ${ASCD_REST_BASE_URL}conductor/v1/anaconda/instances/$ANACONDA_INSTANCE_UUID/environments/$CONDA_ENV_NAME |$PYTHON_BIN -c $'import sys, json\nr="true"\nfor o in json.load(sys.stdin)["operations"]:\n  if (not o["allComplete"]): r="false"\nprint(r)'`
}

getCondaEnvExitStatus() {
	local ANACONDA_INSTANCE_UUID=$1
	local CONDA_ENV_NAME=$2
	getRestUrls
	export CONDA_ENV_EXITSTATUS=`curl -s -S -k -u $EGO_ADMIN_USERNAME:$EGO_ADMIN_PASSWORD -H "Accept:application/json" -X GET ${ASCD_REST_BASE_URL}conductor/v1/anaconda/instances/$ANACONDA_INSTANCE_UUID/environments/$CONDA_ENV_NAME |$PYTHON_BIN -c 'import sys, json; print json.load(sys.stdin)["operations"][0]["activityinfos"][0]["exitstatus"]'`
}

getCondaEnvList() {
	local ANACONDA_INSTANCE_UUID=$1
	getRestUrls
	export CONDA_ENV_LIST=`curl -s -S -k -u $EGO_ADMIN_USERNAME:$EGO_ADMIN_PASSWORD -H "Accept:application/json" -X GET ${ASCD_REST_BASE_URL}conductor/v1/anaconda/instances/$ANACONDA_INSTANCE_UUID/environments |$PYTHON_BIN -c $'import sys, json\nfor name in json.load(sys.stdin):\n print(\'"\'+name+\'"\')'`
}

deleteAnacondaDistribution() {
	local ANACONDA_DISTRIBUTION_ID=$1
	getRestUrls
	local CURL_OUT=`curl -s -S -k -w "%{http_code}" -u $EGO_ADMIN_USERNAME:$EGO_ADMIN_PASSWORD -H "Accept: application/json" -X DELETE ${ASCD_REST_BASE_URL}conductor/v1/anaconda/distributions/$ANACONDA_DISTRIBUTION_ID`
	local RESPONSE=${CURL_OUT:0:(-3)}
	local HTTP_CODE=${CURL_OUT:(-3)}
	if [ "$HTTP_CODE" == "204" ]
	then
		log "Anaconda distribution $ANACONDA_DISTRIBUTION_ID delete operation started started successfully" SUCCESS
	else
		log "Failed to start deletion of Anaconda distribution $ANACONDA_DISTRIBUTION_ID (HTTP CODE $HTTP_CODE). Output of deletion tentative:" WARNING
		log "$RESPONSE" WARNING
	fi
}

##### INSTANCE GROUP MANAGEMENT FUNCTIONS

createIgConsumers() {
	local CONSUMER_BASE_NAME=$1
	local EXECUTION_USER=$2
	local RG_CPU_NAME=$3
	local RG_GPU_NAME=$4
	local ADMIN_USER=$5
	createConsumer "/$CONSUMER_BASE_NAME" "$EXECUTION_USER" $RG_CPU_NAME $RG_GPU_NAME $ADMIN_USER
	createConsumer "/$CONSUMER_BASE_NAME/$CONSUMER_BASE_NAME-MasterServices" "$EXECUTION_USER" $RG_CPU_NAME $RG_GPU_NAME $ADMIN_USER
	createConsumer "/$CONSUMER_BASE_NAME/$CONSUMER_BASE_NAME-NonMasterWorkload" "$EXECUTION_USER" $RG_CPU_NAME $RG_GPU_NAME $ADMIN_USER
	createConsumer "/$CONSUMER_BASE_NAME/$CONSUMER_BASE_NAME-NonMasterWorkload/$CONSUMER_BASE_NAME-Drivers" "$EXECUTION_USER" $RG_CPU_NAME $RG_GPU_NAME $ADMIN_USER
	createConsumer "/$CONSUMER_BASE_NAME/$CONSUMER_BASE_NAME-NonMasterWorkload/$CONSUMER_BASE_NAME-Executors" "$EXECUTION_USER" $RG_CPU_NAME $RG_GPU_NAME $ADMIN_USER
}

updateResourcePlanIgConsumers() {
	local CONSUMER_BASE_NAME=$1
	local RESPLAN_SOURCE=/tmp/resource-plan-source-`date | md5sum | head -c8`.xml
	local RESPLAN_UPDATED=/tmp/resource-plan-updated-`date | md5sum | head -c8`.xml
	authenticate
	log "Updating resource plan for Instance Group in consumer $CONSUMER_BASE_NAME"
	egosh consumer exportresplan $RESPLAN_SOURCE 2>&1 | tee -a $LOG_FILE
	if [ ! -f $RESPLAN_SOURCE ]
	then
		log "Failed to export resource plan, aborting the update of resource plan." ERROR
	else
		$PYTHON_BIN $UPDATE_RP_SCRIPT -i $RESPLAN_SOURCE -o $RESPLAN_UPDATED -c "$CONSUMER_BASE_NAME-NonMasterWorkload,$CONSUMER_BASE_NAME-Executors" 2>&1 | tee -a $LOG_FILE
		if [ ${PIPESTATUS[0]} -ne 0 ]
		then
			log "Failed to modify resource plan, aborting the update of resource plan." ERROR
		else
			egosh consumer applyresplan $RESPLAN_UPDATED 2>&1 | tee -a $LOG_FILE
			if [ ${PIPESTATUS[0]} -ne 0 ]
			then
				log "Failed to apply updated resource plan." ERROR
			else
				log "Resource plan modified successfully" SUCCESS
			fi
		fi
		[[ -f $RESPLAN_SOURCE ]] && rm -f $RESPLAN_SOURCE 2>&1 | tee -a $LOG_FILE
		[[ -f $RESPLAN_UPDATED ]] && rm -f $RESPLAN_UPDATED 2>&1 | tee -a $LOG_FILE
	fi
}

findIgPort() {
	local __resultvar=$1
	local PORT_NAME=$2
	local PORT_DEFAULT=$3
	getRestUrls
	local PORT=`curl -s -S -k -u $EGO_ADMIN_USERNAME:$EGO_ADMIN_PASSWORD -H "Accept:application/json" -X GET ${ASCD_REST_BASE_URL}conductor/v1/instances/parametersinuse |$PYTHON_BIN -c $'import sys,json\nparams=json.load(sys.stdin)\nportMax=0\nfor paramRG in params:\n  if len(paramRG["parametersinuse"]["'$PORT_NAME$'"])>0 and max(paramRG["parametersinuse"]["'$PORT_NAME$'"])>portMax: portMax=max(paramRG["parametersinuse"]["'$PORT_NAME$'"])\nif int(portMax)>0: print int(portMax)+1\nelse: print '$PORT_DEFAULT`
	eval $__resultvar="'$PORT'"
}

findIgPorts() {
	findIgPort SPARK_MASTER_PORT "SPARK_MASTER_PORT" 7077
	findIgPort SPARK_MASTER_WEBUI_PORT "SPARK_MASTER_WEBUI_PORT" 8780
	findIgPort SPARK_MASTER_REST_PORT "spark.master.rest.port" 6066
	findIgPort SPARK_SHUFFLE_SERVICE_PORT "spark.shuffle.service.port" 7337
	findIgPort SPARK_HISTORY_UI_PORT "spark.history.ui.port" 18080
	findIgPort SPARK_EGO_LOGSERVICE_PORT "SPARK_EGO_LOGSERVICE_PORT" 28082
}

createIgSparkJupyter() {
	local IG_PROFILE_TEMPLATE=$1
	local CONSUMER_BASE=$2
	local IG_NAME=$3
	local EXECUTION_USER=$4
	local DEPLOYMENT_BASEDIR=$5
	local SPARKHA_BASEDIR=$6
	local SPARKHISTORY_BASE_DIR=$7
	local NOTEBOOK_BASE_DIR=$8
	local RG_CPU=$9
	local RG_GPU=${10}
	local ANACONDA_INSTANCE_NAME=${11}
	local CONDA_ENV_NAME=${12}
	local SPARKSHUFFLE_SERVICE_ENABLED=${13} # Expect "true" or "false"
	if [ "$SPARKSHUFFLE_SERVICE_ENABLED" == "false" ]
	then
		local SPARKSHUFFLE_BASE_DIR=${14}
	fi
	local TMP_IG_PROFILE=/tmp/ig-profile-`date | md5sum | head -c8`.json
	local CONSUMER_BASE=${CONSUMER_BASE//\//\\\/}
	local DEPLOYMENT_BASEDIR_TMP=${DEPLOYMENT_BASEDIR//\//\\\/}
	local SPARKHA_BASEDIR_TMP=${SPARKHA_BASEDIR//\//\\\/}
	local SPARKHISTORY_BASE_DIR_TMP=${SPARKHISTORY_BASE_DIR//\//\\\/}
	local NOTEBOOK_BASE_DIR_TMP=${NOTEBOOK_BASE_DIR//\//\\\/}
	getRestUrls
	findIgPorts
	getAnacondaInstanceUUID $ANACONDA_INSTANCE_NAME
	log "Preparing IG profile $TMP_IG_PROFILE using template $IG_PROFILE_TEMPLATE"
	cp $IG_PROFILE_TEMPLATE $TMP_IG_PROFILE 2>&1 | tee -a $LOG_FILE
	sed -i 's/##CONSUMER_BASE##/'$CONSUMER_BASE'/g' $TMP_IG_PROFILE 2>&1 | tee -a $LOG_FILE
	sed -i 's/##IG_NAME##/'$IG_NAME'/g' $TMP_IG_PROFILE 2>&1 | tee -a $LOG_FILE
	sed -i 's/##RG_CPU##/'$RG_CPU'/g' $TMP_IG_PROFILE 2>&1 | tee -a $LOG_FILE
	if [ "$RG_CPU" == "$RG_GPU" ]
	then
		sed -i 's/"executor_gpu_rg_param":"##RG_GPU##"\,//g' $TMP_IG_PROFILE 2>&1 | tee -a $LOG_FILE
	else
		sed -i 's/##RG_GPU##/'$RG_GPU'/g' $TMP_IG_PROFILE 2>&1 | tee -a $LOG_FILE
	fi
	sed -i 's/##EXECUTION_USER##/'$EXECUTION_USER'/g' $TMP_IG_PROFILE 2>&1 | tee -a $LOG_FILE
	sed -i 's/##DEPLOYMENT_BASEDIR##/'$DEPLOYMENT_BASEDIR_TMP'/g' $TMP_IG_PROFILE 2>&1 | tee -a $LOG_FILE
	sed -i 's/##SPARKHA_BASEDIR##/'$SPARKHA_BASEDIR_TMP'/g' $TMP_IG_PROFILE 2>&1 | tee -a $LOG_FILE
	sed -i 's/##SPARKHISTORY_BASE_DIR##/'$SPARKHISTORY_BASE_DIR_TMP'/g' $TMP_IG_PROFILE 2>&1 | tee -a $LOG_FILE
	sed -i 's/##SPARK_MASTER_PORT##/'$SPARK_MASTER_PORT'/g' $TMP_IG_PROFILE 2>&1 | tee -a $LOG_FILE
	sed -i 's/##SPARK_MASTER_WEBUI_PORT##/'$SPARK_MASTER_WEBUI_PORT'/g' $TMP_IG_PROFILE 2>&1 | tee -a $LOG_FILE
	sed -i 's/##SPARK_MASTER_REST_PORT##/'$SPARK_MASTER_REST_PORT'/g' $TMP_IG_PROFILE 2>&1 | tee -a $LOG_FILE
	if [ "$SPARKSHUFFLE_SERVICE_ENABLED" == "true" ]
	then
		sed -i 's/##SPARK_SHUFFLE_SERVICE_PORT##/'$SPARK_SHUFFLE_SERVICE_PORT'/g' $TMP_IG_PROFILE 2>&1 | tee -a $LOG_FILE
	else
		local SPARK_LOCAL_DIR=$SPARKSHUFFLE_BASE_DIR/$IG_NAME
		sed -i 's#"spark.shuffle.service.port.*$#"spark.shuffle.service.enabled": "false",\n              "spark.local.dir": "'$SPARK_LOCAL_DIR'",#' $TMP_IG_PROFILE 2>&1 | tee -a $LOG_FILE
	fi
	sed -i 's/##SPARK_HISTORY_UI_PORT##/'$SPARK_HISTORY_UI_PORT'/g' $TMP_IG_PROFILE 2>&1 | tee -a $LOG_FILE
	sed -i 's/##SPARK_EGO_LOGSERVICE_PORT##/'$SPARK_EGO_LOGSERVICE_PORT'/g' $TMP_IG_PROFILE 2>&1 | tee -a $LOG_FILE
	sed -i 's/##NOTEBOOK_BASE_DIR##/'$NOTEBOOK_BASE_DIR_TMP'/g' $TMP_IG_PROFILE 2>&1 | tee -a $LOG_FILE
	sed -i 's/##CONDA_ENV_NAME##/'$CONDA_ENV_NAME'/g' $TMP_IG_PROFILE 2>&1 | tee -a $LOG_FILE
	sed -i 's/##ANACONDA_INSTANCE_NAME##/'$ANACONDA_INSTANCE_NAME'/g' $TMP_IG_PROFILE 2>&1 | tee -a $LOG_FILE
	sed -i 's/##ANACONDA_INSTANCE_UUID##/'$ANACONDA_INSTANCE_UUID'/g' $TMP_IG_PROFILE 2>&1 | tee -a $LOG_FILE
	registerIg $TMP_IG_PROFILE
	rm -f $TMP_IG_PROFILE
	deployIg $IG_UUID
	startIg $IG_UUID
}

createIgDliEdt() {
	local IG_PROFILE_TEMPLATE=$1
	local CONSUMER_BASE=$2
	local IG_NAME=$3
	local EXECUTION_USER=$4
	local DEPLOYMENT_BASEDIR=$5
	local SPARKHA_BASEDIR=$6
	local SPARKHISTORY_BASE_DIR=$7
	local RG_CPU=$8
	local RG_GPU=${9}
	local DLI_CONDA_ENV_NAME=${10}
	local DLI_SHAREDFS_CONF_DIR=${11}
	local DLI_CONDA_HOME=${12}
	local SPARKSHUFFLE_SERVICE_ENABLED=${13} # Expect "true" or "false"
	if [ "$SPARKSHUFFLE_SERVICE_ENABLED" == "false" ]
	then
		local SPARKSHUFFLE_BASE_DIR=${14}
	fi
	local TMP_IG_PROFILE=/tmp/ig-profile-`date | md5sum | head -c8`.json
	local CONSUMER_BASE=${CONSUMER_BASE//\//\\\/}
	local DEPLOYMENT_BASEDIR_TMP=${DEPLOYMENT_BASEDIR//\//\\\/}
	local SPARKHA_BASEDIR_TMP=${SPARKHA_BASEDIR//\//\\\/}
	local SPARKHISTORY_BASE_DIR_TMP=${SPARKHISTORY_BASE_DIR//\//\\\/}
	local DLI_SHAREDFS_CONF_DIR_TMP=${DLI_SHAREDFS_CONF_DIR//\//\\\/}
	local DLI_CONDA_HOME_TMP=${DLI_CONDA_HOME//\//\\\/}
	getRestUrls
	findIgPorts
	log "Preparing IG profile $TMP_IG_PROFILE using template $IG_PROFILE_TEMPLATE"
	cp $IG_PROFILE_TEMPLATE $TMP_IG_PROFILE 2>&1 | tee -a $LOG_FILE
	sed -i 's/##CONSUMER_BASE##/'$CONSUMER_BASE'/g' $TMP_IG_PROFILE 2>&1 | tee -a $LOG_FILE
	sed -i 's/##IG_NAME##/'$IG_NAME'/g' $TMP_IG_PROFILE 2>&1 | tee -a $LOG_FILE
	sed -i 's/##RG_CPU##/'$RG_CPU'/g' $TMP_IG_PROFILE 2>&1 | tee -a $LOG_FILE
	if [ "$RG_CPU" == "$RG_GPU" ]
	then
		sed -i 's/"executor_gpu_rg_param":"##RG_GPU##"\,//g' $TMP_IG_PROFILE 2>&1 | tee -a $LOG_FILE
		sed -i 's/"SPARK_EGO_AUTOSCALE_GPU_SLOTS_PER_TASK": "1"\,//g' $TMP_IG_PROFILE 2>&1 | tee -a $LOG_FILE
	else
		sed -i 's/##RG_GPU##/'$RG_GPU'/g' $TMP_IG_PROFILE 2>&1 | tee -a $LOG_FILE
	fi
	sed -i 's/##EXECUTION_USER##/'$EXECUTION_USER'/g' $TMP_IG_PROFILE 2>&1 | tee -a $LOG_FILE
	sed -i 's/##DEPLOYMENT_BASEDIR##/'$DEPLOYMENT_BASEDIR_TMP'/g' $TMP_IG_PROFILE 2>&1 | tee -a $LOG_FILE
	sed -i 's/##SPARKHA_BASEDIR##/'$SPARKHA_BASEDIR_TMP'/g' $TMP_IG_PROFILE 2>&1 | tee -a $LOG_FILE
	sed -i 's/##SPARKHISTORY_BASE_DIR##/'$SPARKHISTORY_BASE_DIR_TMP'/g' $TMP_IG_PROFILE 2>&1 | tee -a $LOG_FILE
	sed -i 's/##SPARK_MASTER_PORT##/'$SPARK_MASTER_PORT'/g' $TMP_IG_PROFILE 2>&1 | tee -a $LOG_FILE
	sed -i 's/##SPARK_MASTER_WEBUI_PORT##/'$SPARK_MASTER_WEBUI_PORT'/g' $TMP_IG_PROFILE 2>&1 | tee -a $LOG_FILE
	sed -i 's/##SPARK_MASTER_REST_PORT##/'$SPARK_MASTER_REST_PORT'/g' $TMP_IG_PROFILE 2>&1 | tee -a $LOG_FILE
	if [ "$SPARKSHUFFLE_SERVICE_ENABLED" == "true" ]
	then
		sed -i 's/##SPARK_SHUFFLE_SERVICE_PORT##/'$SPARK_SHUFFLE_SERVICE_PORT'/g' $TMP_IG_PROFILE 2>&1 | tee -a $LOG_FILE
	else
		local SPARK_LOCAL_DIR=$SPARKSHUFFLE_BASE_DIR/$IG_NAME
		sed -i 's#"spark.shuffle.service.port.*$#"spark.shuffle.service.enabled": "false",\n              "spark.local.dir": "'$SPARK_LOCAL_DIR'",#' $TMP_IG_PROFILE 2>&1 | tee -a $LOG_FILE
	fi
	sed -i 's/##SPARK_HISTORY_UI_PORT##/'$SPARK_HISTORY_UI_PORT'/g' $TMP_IG_PROFILE 2>&1 | tee -a $LOG_FILE
	sed -i 's/##SPARK_EGO_LOGSERVICE_PORT##/'$SPARK_EGO_LOGSERVICE_PORT'/g' $TMP_IG_PROFILE 2>&1 | tee -a $LOG_FILE
	sed -i 's/##CONDA_ENV_NAME##/'$DLI_CONDA_ENV_NAME'/g' $TMP_IG_PROFILE 2>&1 | tee -a $LOG_FILE
	sed -i 's/##DLI_CONDA_HOME##/'$DLI_CONDA_HOME_TMP'/g' $TMP_IG_PROFILE 2>&1 | tee -a $LOG_FILE
	sed -i 's/##DLI_SHAREDFS_CONF_DIR##/'$DLI_SHAREDFS_CONF_DIR_TMP'/g' $TMP_IG_PROFILE 2>&1 | tee -a $LOG_FILE
	registerIg $TMP_IG_PROFILE
	rm -f $TMP_IG_PROFILE
	deployIg $IG_UUID
	startIg $IG_UUID
}

registerIg() {
	local IG_PROFILE=$1
	getRestUrls
	log "Registering Instance Group"
	local IG_REGISTRATION_OUTPUT=`curl -s -S -k -u $EGO_ADMIN_USERNAME:$EGO_ADMIN_PASSWORD -H "Content-Type:application/json" -H "Accept:application/json" -X POST --data-binary @$IG_PROFILE ${ASCD_REST_BASE_URL}conductor/v1/instances/`
	export IG_UUID=`echo $IG_REGISTRATION_OUTPUT | sed -e 's/"//g'`
	if [ ${#IG_UUID} -ne 36 ]
	then
		log "Failed to register Instance Group, aborting. Output of registration tentative:" ERROR
		log "$IG_REGISTRATION_OUTPUT" ERROR
		exit 1
	fi
	log "Instance Group registered successfully" SUCCESS
}

deployIg() {
	local IG_UUID=$1
	getRestUrls
	log "Deploying Instance Group"
	local CURL_OUT=`curl -s -S -k -w "%{http_code}" -u $EGO_ADMIN_USERNAME:$EGO_ADMIN_PASSWORD -X PUT ${ASCD_REST_BASE_URL}conductor/v1/instances/$IG_UUID/deploy?timeout=$IG_DEPLOY_TIMEOUT`
	local RESPONSE=${CURL_OUT:0:(-3)}
	local HTTP_CODE=${CURL_OUT:(-3)}
	if [ "$HTTP_CODE" == "204" ]
	then
		log "Instance Group $IG_UUID deployment started successfully" SUCCESS
	else
		log "Failed to start deployment of Instance Group $IG_UUID (HTTP CODE $HTTP_CODE), aborting. Output of deployment tentative:" ERROR
		log "$RESPONSE" ERROR
		exit 1
	fi
	getIgState $IG_UUID
	while [ "$IG_STATE" != "READY" ]
	do
		if [ "$IG_STATE" == "DEPLOY_ERROR" ]
		then
			log "Deployment failed (Instance Group state: $IG_STATE)" ERROR
			exit 1
		fi
		log "Instance Group deploying (state: $IG_STATE) ..."
		sleep $STATUS_CHECK_WAITTIME
		getIgState $IG_UUID
	done
	log "Instance Group deployed successfully" SUCCESS
}

startIg() {
	local IG_UUID=$1
	getRestUrls
	log "Starting Instance Group"
	local CURL_OUT=`curl -s -S -k -w "%{http_code}" -u $EGO_ADMIN_USERNAME:$EGO_ADMIN_PASSWORD -X PUT ${ASCD_REST_BASE_URL}conductor/v1/instances/$IG_UUID/start`
	local RESPONSE=${CURL_OUT:0:(-3)}
	local HTTP_CODE=${CURL_OUT:(-3)}
	if [ "$HTTP_CODE" == "204" ]
	then
		log "Instance Group $IG_UUID start operation started successfully" SUCCESS
	else
		log "Failed to start Instance Group $IG_UUID (HTTP CODE $HTTP_CODE). Output of start tentative:" WARNING
		log "$RESPONSE" WARNING
	fi
	getIgState $IG_UUID
	while [ "$IG_STATE" != "STARTED" ]
	do
		if [ "$IG_STATE" == "ERROR" ]
		then
			log "Failed to start Instance Group (state: $IG_STATE)" WARNING
		fi
		log "Instance Group starting (state: $IG_STATE) ..."
		sleep $STATUS_CHECK_WAITTIME
		getIgState $IG_UUID
	done
	log "Instance Group started" SUCCESS
}

stopIg() {
	local IG_UUID=$1
	getRestUrls
	log "Stopping Instance Group"
	local CURL_OUT=`curl -s -S -k -w "%{http_code}" -u $EGO_ADMIN_USERNAME:$EGO_ADMIN_PASSWORD -X PUT ${ASCD_REST_BASE_URL}conductor/v1/instances/$IG_UUID/stop`
	local RESPONSE=${CURL_OUT:0:(-3)}
	local HTTP_CODE=${CURL_OUT:(-3)}
	if [ "$HTTP_CODE" == "204" ]
	then
		log "Instance Group $IG_UUID stop operation started successfully" SUCCESS
	else
		log "Failed to stop Instance Group $IG_UUID (HTTP CODE $HTTP_CODE). Output of stop tentative:" WARNING
		log "$RESPONSE" WARNING
	fi
	getIgState $IG_UUID
	while [ "$IG_STATE" != "READY" ]
	do
		log "Instance Group stopping (state: $IG_STATE) ..."
		sleep $STATUS_CHECK_WAITTIME
		getIgState $IG_UUID
	done
	log "Instance Group stopped" SUCCESS
}

createIgNotebookInstance() {
	local NOTEBOOK_INSTANCE_USER=$1
	local IG_UUID=$2
	local NOTEBOOK_NAME=$3
	local NOTEBOOK_VERSION=$4
	getRestUrls
	local CURL_OUT=`curl -s -S -k -w "%{http_code}" -u $EGO_ADMIN_USERNAME:$EGO_ADMIN_PASSWORD -H "Content-Type:application/json" -H "Accept:application/json" -X POST --data-binary '{"userlist": [ "'$NOTEBOOK_INSTANCE_USER'" ], "notebookinfolist": [ { "notebook": "'$NOTEBOOK_NAME'", "version": "'$NOTEBOOK_VERSION'" } ] }' ${ASCD_REST_BASE_URL}conductor/v1/instances/$IG_UUID/notebooks`
	local RESPONSE=${CURL_OUT:0:(-3)}
	local HTTP_CODE=${CURL_OUT:(-3)}
	if [ "$HTTP_CODE" == "201" ]
	then
		log "Notebook instance for user $NOTEBOOK_INSTANCE_USER on Instance Group $IG_UUID and notebook $NOTEBOOK_NAME $NOTEBOOK_VERSION created successfully" SUCCESS
	else
		log "Failed to create Notebook instance for user $NOTEBOOK_INSTANCE_USER on Instance Group $IG_UUID and notebook $NOTEBOOK_NAME $NOTEBOOK_VERSION (HTTP CODE $HTTP_CODE). Output of creation tentative:" WARNING
		log "$RESPONSE" WARNING
	fi
}

getIgState() {
	local IG_UUID=$1
	getRestUrls
	export IG_STATE=`curl -s -S -k -u $EGO_ADMIN_USERNAME:$EGO_ADMIN_PASSWORD -H "Accept:application/json" -X GET ${ASCD_REST_BASE_URL}conductor/v1/instances/?id=$IG_UUID |$PYTHON_BIN -c 'import sys, json; print json.load(sys.stdin)[0]["state"]'`
}

getIgUUID() {
	local IG_NAME=$1
	getRestUrls
	export IG_UUID=`curl -s -S -k -u $EGO_ADMIN_USERNAME:$EGO_ADMIN_PASSWORD -H "Accept:application/json" -X GET ${ASCD_REST_BASE_URL}conductor/v1/instances/?name=$IG_NAME | $PYTHON_BIN -c 'import sys, json; print json.load(sys.stdin)[0]["id"]["uuid"]'`
}
