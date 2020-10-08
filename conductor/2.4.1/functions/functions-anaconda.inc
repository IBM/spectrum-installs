#!/bin/sh

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
		convertCondaTemplateToProfile $ANACONDA_ENV_PROFILE env${ANACONDA_ENV_ID} TMP_CONDA_ENV_PROFILE
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
	mv $ANACONDA_INSTALL_DIR/pkgs/*.conda $ANACONDA_LOCAL_CHANNEL_ARCH_DIR 2>&1 | tee -a $LOG_FILE
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
	log "Creating the archive $ANACONDA_LOCAL_CHANNEL_ARCHIVE with the local conda channel $ANACONDA_LOCAL_CHANNEL_DIR"
	tar czf $ANACONDA_LOCAL_CHANNEL_ARCHIVE -C $ANACONDA_LOCAL_CHANNEL_DIR . 2>&1 | tee -a $LOG_FILE
	CODE=${PIPESTATUS[0]}
	if [ $CODE -ne 0 ]
	then
		log "Failed to create the archive containing the local conda channel (exit code: $CODE), aborting" WARNING
	else
		log "Archive containing the local conda channel created successfully" SUCCESS
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
		prepareDir $ANACONDA_LOCAL_CHANNEL_DIR $CLUSTERADMIN
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
	CONDA_ENV_PROFILE_TEMPLATE_UPDATED=/tmp/condaEnv-profile-localChannel-`date +%s%N | md5sum | head -c8`.yaml
	head -n 1 $CONDA_ENV_PROFILE_TEMPLATE_SOURCE > $CONDA_ENV_PROFILE_TEMPLATE_UPDATED
	echo "channels:" >> $CONDA_ENV_PROFILE_TEMPLATE_UPDATED
	echo "  - file://$ANACONDA_LOCAL_CHANNEL_DIR" >> $CONDA_ENV_PROFILE_TEMPLATE_UPDATED
	if [ "$ANACONDA_LOCAL_CHANNEL_STRICT" == "enabled" ]
	then
		echo "  - nodefaults" >> $CONDA_ENV_PROFILE_TEMPLATE_UPDATED
	fi
	sed -n '/^dependencies\:/,$p' $CONDA_ENV_PROFILE_TEMPLATE_SOURCE >> $CONDA_ENV_PROFILE_TEMPLATE_UPDATED
	eval $__resultvar="'$CONDA_ENV_PROFILE_TEMPLATE_UPDATED'"
}

createAirgapCondaInstall() {
	local ANACONDA_INSTALL_DIR=$1
	local ANACONDA_ENV_PROFILES_LIST=$2
	local ANACONDA_INSTALL_ARCHIVE=$3
	local ANACONDA_DISTRIBUTION_FILE=$CACHE_DIR/${ANACONDA_AIRGAP_DISTRIBUTION_NAME}.sh
	local ANACONDA_ENV_ID=0
	downloadAnacondaDistribution $ANACONDA_AIRGAP_DISTRIBUTION_NAME $ANACONDA_DISTRIBUTION_FILE
	if [ -d "$ANACONDA_INSTALL_DIR" ]
	then
		log "Deleting previous install of Anaconda in $ANACONDA_INSTALL_DIR"
		rm -rf $ANACONDA_INSTALL_DIR 2>&1 | tee -a $LOG_FILE
	fi
	log "Installing Anaconda from distribution $ANACONDA_AIRGAP_DISTRIBUTION_NAME in directory $ANACONDA_INSTALL_DIR"
	bash $ANACONDA_DISTRIBUTION_FILE -b -p $ANACONDA_INSTALL_DIR 2>&1 | tee -a $LOG_FILE
	if [ ${PIPESTATUS[0]} -ne 0 ]
	then
		log "Failed to install Anaconda from $ANACONDA_AIRGAP_DISTRIBUTION_NAME into $ANACONDA_INSTALL_DIR, aborting" ERROR
		exit 1
	fi
	log "Activating Anaconda"
	. $ANACONDA_INSTALL_DIR/etc/profile.d/conda.sh
	export IBM_POWERAI_LICENSE_ACCEPT=yes
	for ANACONDA_ENV_PROFILE in $ANACONDA_ENV_PROFILES_LIST
	do
		if [ ! -f $ANACONDA_ENV_PROFILE ]
		then
			log "Profile $ANACONDA_ENV_PROFILE doesn't exist, aborting" ERROR
			exit 1
		fi
		parseCondaEnvProfile $ANACONDA_ENV_PROFILE
		local ATTEMPT_NB=1
		while [ $ATTEMPT_NB -le $ANACONDA_AIRGAP_CONDA_ENV_CREATION_NB_ATTEMPT ]
		do
			log "Creating conda environment $CONDA_ENV_PROFILE_PARSED_NAME using profile $ANACONDA_ENV_PROFILE (attempt #$ATTEMPT_NB)"
			$ANACONDA_INSTALL_DIR/bin/conda env create -f $ANACONDA_ENV_PROFILE 2>&1 | tee -a $LOG_FILE
			CODE=${PIPESTATUS[0]}
			if [ $CODE -eq 0 ]
			then
				log "Conda environment $CONDA_ENV_PROFILE_PARSED_NAME created successfully" SUCCESS
				break
			else
				if [ $ATTEMPT_NB -eq $ANACONDA_AIRGAP_CONDA_ENV_CREATION_NB_ATTEMPT ]
				then
					log "Failed to create conda environment $CONDA_ENV_PROFILE_PARSED_NAME from profile $ANACONDA_ENV_PROFILE (exit code: $CODE), aborting" ERROR
					exit 1
				else
					log "Failed to create conda environment $CONDA_ENV_PROFILE_PARSED_NAME from profile $ANACONDA_ENV_PROFILE (exit code: $CODE)" WARNING
					log "Deleting conda environment $CONDA_ENV_PROFILE_PARSED_NAME"
					$ANACONDA_INSTALL_DIR/bin/conda env remove -n $CONDA_ENV_PROFILE_PARSED_NAME 2>&1 | tee -a $LOG_FILE
					log "Waiting $ANACONDA_AIRGAP_CONDA_ENV_CREATION_WAITTIME seconds before trying again"
					sleep $ANACONDA_AIRGAP_CONDA_ENV_CREATION_WAITTIME
				fi
			fi
			ATTEMPT_NB=$((ATTEMPT_NB+1))
		done
	done
	log "Creating the archive $ANACONDA_INSTALL_ARCHIVE with the conda environments directory $ANACONDA_INSTALL_DIR/envs"
	tar czf $ANACONDA_INSTALL_ARCHIVE -C $ANACONDA_INSTALL_DIR/envs . 2>&1 | tee -a $LOG_FILE
	CODE=${PIPESTATUS[0]}
	if [ $CODE -ne 0 ]
	then
		log "Failed to create the archive containing the conda environments directory (exit code: $CODE), aborting" WARNING
	else
		log "Archive containing the conda environments directory created successfully" SUCCESS
	fi
	log "Deleting Anaconda installation directory"
	rm -rf $ANACONDA_INSTALL_DIR 2>&1 | tee -a $LOG_FILE
}

extractCondaEnvironmentsFromArchive() {
	local ANACONDA_INSTALL_ARCHIVE=$1
	local ANACONDA_INSTALL_DIR=$2
	local CONDA_ENV_NAMES=$3
	if [ ! -f "$ANACONDA_INSTALL_ARCHIVE" ]
	then
		log "Anaconda instance archive $ANACONDA_INSTALL_ARCHIVE doesn't exist, aborting." ERROR
		exit 1
	fi
	if [ ! -d "$ANACONDA_INSTALL_DIR" ]
	then
		log "Anaconda instance directory $ANACONDA_INSTALL_DIR doesn't exist, aborting." ERROR
		exit 1
	fi
	for CONDA_ENV_NAME in $CONDA_ENV_NAMES
	do
		if [ -d "$ANACONDA_INSTALL_DIR/envs/$CONDA_ENV_NAME" ]
		then
			log "Directory for conda environment $CONDA_ENV_NAME already exist, aborting." ERROR
			exit 1
		fi
	done
	log "Extracting archive $ANACONDA_INSTALL_ARCHIVE in $ANACONDA_INSTALL_DIR/envs"
	tar xzf $ANACONDA_INSTALL_ARCHIVE -C $ANACONDA_INSTALL_DIR/envs 2>&1 | tee -a $LOG_FILE
	CODE=${PIPESTATUS[0]}
	if [ $CODE -ne 0 ]
	then
		log "Failed to extract conda environments from archive $ANACONDA_INSTALL_ARCHIVE in directory $ANACONDA_INSTALL_DIR/envs (exit code: $CODE), aborting" ERROR
		exit 1
	fi
	log "Changing ownership of $ANACONDA_INSTALL_DIR/envs to $CLUSTERADMIN"
	chown -R $CLUSTERADMIN:$CLUSTERADMIN $ANACONDA_INSTALL_DIR/envs 2>&1 | tee -a $LOG_FILE
	log "Conda environments extracted succesfully" SUCCESS
}

discoverCondaEnvironments() {
	local ANACONDA_INSTALL_DIR=$1
	local CONDA_ENV_NAMES=$2
	local ANACONDA_INSTANCE_UUID=$3
	for CONDA_ENV_NAME in $CONDA_ENV_NAMES
	do
		local ATTEMPT_NB=1
		while [ $ATTEMPT_NB -le $CONDA_ENV_CREATION_NB_ATTEMPT ]
		do
			log "Discovering conda environment $CONDA_ENV_NAME from directory $ANACONDA_INSTALL_DIR/envs/$CONDA_ENV_NAME (attempt #$ATTEMPT_NB)"
			discoverCondaEnvironment $ANACONDA_INSTANCE_UUID $CONDA_ENV_NAME
			waitForCondaEnvironmentDeployed $ANACONDA_INSTANCE_UUID $CONDA_ENV_NAME
			if [ $CONDA_ENV_EXITSTATUS -eq 0 ]
			then
				log "Conda environment $CONDA_ENV_NAME discovered succesfully" SUCCESS
				break
			else
				if [ $ATTEMPT_NB -eq $CONDA_ENV_CREATION_NB_ATTEMPT ]
				then
					log "Conda environment $CONDA_ENV_NAME discovery failed $ATTEMPT_NB times (last exit status: $CONDA_ENV_EXITSTATUS), aborting." ERROR
					exit 1
				fi
			fi
			ATTEMPT_NB=$((ATTEMPT_NB+1))
		done
	done
}

discoverCondaEnvironment() {
	local ANACONDA_INSTANCE_UUID=$1
	local CONDA_ENV_NAME=$2
	getRestUrls
	local CURL_OUT=`curl -s -S -k -w "%{http_code}" -u $EGO_ADMIN_USERNAME:$EGO_ADMIN_PASSWORD -X PUT -H 'Content-Type:application/json' -H 'Accept:application/json' --data @$CONDA_ENV_PACKAGES_LIST_FILE ${ASCD_REST_BASE_URL}conductor/v1/anaconda/instances/$ANACONDA_INSTANCE_UUID/environments/$CONDA_ENV_NAME/refresh`
	local RESPONSE=${CURL_OUT:0:(-3)}
	local HTTP_CODE=${CURL_OUT:(-3)}
	if [ "$HTTP_CODE" == "204" ]
	then
		log "Discovery of conda environment $CONDA_ENV_NAME started successfully" SUCCESS
	else
		log "Failed to start discovery of conda environment $CONDA_ENV_NAME (HTTP CODE $HTTP_CODE), aborting. Output of discovery tentative:" ERROR
		log "$RESPONSE" ERROR
		exit 1
	fi
}

getAnacondaDistributions() {
	getRestUrls
	local CURL_OUT=`curl -s -S -k -w "%{http_code}" -u $EGO_ADMIN_USERNAME:$EGO_ADMIN_PASSWORD -X GET -H 'Accept:application/json' ${ASCD_REST_BASE_URL}conductor/v1/anaconda/distributions`
	local RESPONSE=${CURL_OUT:0:(-3)}
	local HTTP_CODE=${CURL_OUT:(-3)}
	if [ "$HTTP_CODE" == "200" ]
	then
		export ANACONDA_DISTRIBUTIONS=$RESPONSE
	else
		log "Failed to retrieve Anaconda distributions (HTTP CODE $HTTP_CODE), aborting. Output of REST call:" ERROR
		log "$RESPONSE" ERROR
		exit 1
	fi
}

getAnacondaDistribution() {
	local ANACONDA_DISTRIBUTION_ID=$1
	getRestUrls
	local CURL_OUT=`curl -s -S -k -w "%{http_code}" -u $EGO_ADMIN_USERNAME:$EGO_ADMIN_PASSWORD -X GET -H 'Accept:application/json' ${ASCD_REST_BASE_URL}conductor/v1/anaconda/distributions/$ANACONDA_DISTRIBUTION_ID`
	local RESPONSE=${CURL_OUT:0:(-3)}
	local HTTP_CODE=${CURL_OUT:(-3)}
	if [ "$HTTP_CODE" == "200" ]
	then
		export ANACONDA_DISTRIBUTION_DETAILS=$RESPONSE
	else
		log "Failed to retrieve Anaconda distribution with id $ANACONDA_DISTRIBUTION_ID (HTTP CODE $HTTP_CODE), aborting. Output of REST call:" ERROR
		log "$RESPONSE" ERROR
		exit 1
	fi
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

getAnacondaInstances(){
	getRestUrls
	local CURL_OUT=`curl -s -S -k -w "%{http_code}" -u $EGO_ADMIN_USERNAME:$EGO_ADMIN_PASSWORD -X GET -H 'Accept:application/json' ${ASCD_REST_BASE_URL}conductor/v1/anaconda/instances`
	local RESPONSE=${CURL_OUT:0:(-3)}
	local HTTP_CODE=${CURL_OUT:(-3)}
	if [ "$HTTP_CODE" == "200" ]
	then
		export ANACONDA_INSTANCES=$RESPONSE
	else
		log "Failed to retrieve Anaconda instances (HTTP CODE $HTTP_CODE), aborting. Output of REST call:" ERROR
		log "$RESPONSE" ERROR
		exit 1
	fi
}

getAnacondaInstance() {
	local ANACONDA_INSTANCE_ID=$1
	getRestUrls
	local CURL_OUT=`curl -s -S -k -w "%{http_code}" -u $EGO_ADMIN_USERNAME:$EGO_ADMIN_PASSWORD -X GET -H 'Accept:application/json' ${ASCD_REST_BASE_URL}conductor/v1/anaconda/instances/$ANACONDA_INSTANCE_ID`
	local RESPONSE=${CURL_OUT:0:(-3)}
	local HTTP_CODE=${CURL_OUT:(-3)}
	if [ "$HTTP_CODE" == "200" ]
	then
		export ANACONDA_INSTANCE_DETAILS=$RESPONSE
	else
		log "Failed to retrieve Anaconda instance with id $ANACONDA_INSTANCE_ID (HTTP CODE $HTTP_CODE), aborting. Output of REST call:" ERROR
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

createCondaEnvironment() {
	local ANACONDA_INSTANCE_UUID=$1
	local CONDA_ENV_PROFILE_TEMPLATE=$2
	local CONDA_ENV_NAME=$3
	if [ "$ANACONDA_LOCAL_CONDA_ENV_CREATION_TYPE" == "twosteps" ]
	then
		createCondaEnvironmentInTwoStepsAndWait $ANACONDA_INSTANCE_UUID $CONDA_ENV_PROFILE_TEMPLATE $CONDA_ENV_NAME
	else
		createCondaEnvironmentFromProfileAndWait $ANACONDA_INSTANCE_UUID $CONDA_ENV_PROFILE_TEMPLATE $CONDA_ENV_NAME
	fi
}

createCondaEnvironmentFromProfileAndWait() {
	local ANACONDA_INSTANCE_UUID=$1
	local CONDA_ENV_PROFILE_TEMPLATE=$2
	local CONDA_ENV_NAME=$3
	local ATTEMPT_NB=1
	while [ $ATTEMPT_NB -le $CONDA_ENV_CREATION_NB_ATTEMPT ]
	do
		log "Creating conda environment $CONDA_ENV_NAME from profile template $CONDA_ENV_PROFILE_TEMPLATE (attempt #$ATTEMPT_NB)"
		createCondaEnvironmentFromProfile $ANACONDA_INSTANCE_UUID $CONDA_ENV_PROFILE_TEMPLATE $CONDA_ENV_NAME
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

createCondaEnvironmentInTwoStepsAndWait() {
	local ANACONDA_INSTANCE_UUID=$1
	local CONDA_ENV_PROFILE_TEMPLATE=$2
	local CONDA_ENV_NAME=$3
	local ATTEMPT_NB=1
	while [ $ATTEMPT_NB -le $CONDA_ENV_CREATION_NB_ATTEMPT ]
	do
		log "Creating empty conda environment $CONDA_ENV_NAME (attempt #$ATTEMPT_NB)"
		createEmptyCondaEnvironment $ANACONDA_INSTANCE_UUID $CONDA_ENV_NAME
		waitForCondaEnvironmentDeployed $ANACONDA_INSTANCE_UUID $CONDA_ENV_NAME
		if [ $CONDA_ENV_EXITSTATUS -ne 0 ]
		then
			if [ $ATTEMPT_NB -eq $CONDA_ENV_CREATION_NB_ATTEMPT ]
			then
				log "Conda environment $CONDA_ENV_NAME creation failed $ATTEMPT_NB times (last exit status: $CONDA_ENV_EXITSTATUS), aborting." ERROR
				exit 1
			else
				log "Conda environment $CONDA_ENV_NAME creation failed (exit status: $CONDA_ENV_EXITSTATUS)" WARNING
				ATTEMPT_NB=$((ATTEMPT_NB+1))
				continue
			fi
		else
			installPackagesInCondaEnvironment $ANACONDA_INSTANCE_UUID $CONDA_ENV_NAME $CONDA_ENV_PROFILE_TEMPLATE
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
		fi
		ATTEMPT_NB=$((ATTEMPT_NB+1))
	done
}

createCondaEnvironmentFromProfile() {
	local ANACONDA_INSTANCE_UUID=$1
	local CONDA_ENV_PROFILE_TEMPLATE=$2
	local CONDA_ENV_NAME=$3
	getRestUrls
	convertCondaTemplateToProfile $CONDA_ENV_PROFILE_TEMPLATE $CONDA_ENV_NAME TMP_CONDA_ENV_PROFILE
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

createEmptyCondaEnvironment() {
	local ANACONDA_INSTANCE_UUID=$1
	local CONDA_ENV_NAME=$2
	getRestUrls
	local CURL_OUT=`curl -s -S -k -w "%{http_code}" -u $EGO_ADMIN_USERNAME:$EGO_ADMIN_PASSWORD -X POST -H 'Accept:application/json' ${ASCD_REST_BASE_URL}conductor/v1/anaconda/instances/$ANACONDA_INSTANCE_UUID/environments?name=$CONDA_ENV_NAME`
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
}

installPackagesInCondaEnvironment() {
	local ANACONDA_INSTANCE_UUID=$1
	local CONDA_ENV_NAME=$2
	local CONDA_ENV_PROFILE_TEMPLATE=$3
	createJsonPackagesToInstallInCondaEnv $CONDA_ENV_PROFILE_TEMPLATE CONDA_ENV_PACKAGES_LIST_FILE
	getRestUrls
	local CURL_OUT=`curl -s -S -k -w "%{http_code}" -u $EGO_ADMIN_USERNAME:$EGO_ADMIN_PASSWORD -X PUT -H 'Content-Type:application/json' -H 'Accept:application/json' --data @$CONDA_ENV_PACKAGES_LIST_FILE ${ASCD_REST_BASE_URL}conductor/v1/anaconda/instances/$ANACONDA_INSTANCE_UUID/environments/$CONDA_ENV_NAME/install`
	local RESPONSE=${CURL_OUT:0:(-3)}
	local HTTP_CODE=${CURL_OUT:(-3)}
	if [ "$HTTP_CODE" == "204" ]
	then
		log "Installation of packages in conda environment $CONDA_ENV_NAME started successfully" SUCCESS
		rm -f $CONDA_ENV_PACKAGES_LIST_FILE 2>&1 | tee -a $LOG_FILE
	else
		log "Failed to start installation of packages in conda environment $CONDA_ENV_NAME (HTTP CODE $HTTP_CODE), aborting. Output of installation tentative:" ERROR
		log "$RESPONSE" ERROR
		exit 1
	fi
}

createJsonPackagesToInstallInCondaEnv() {
	local CONDA_ENV_PROFILE_TEMPLATE=$1
	local __resultvar=$2
	local CONDA_INSTALL_PACKAGES=""
	CONDA_ENV_PACKAGES_LIST_FILE=/tmp/conda-install-packages-localChannel-`date +%s%N | md5sum | head -c8`.json
	parseCondaEnvProfile $CONDA_ENV_PROFILE_TEMPLATE
	for CONDA_ENV_PROFILE_PARSED_PACKAGE in $CONDA_ENV_PROFILE_PARSED_PACKAGES
	do
		CONDA_INSTALL_PACKAGES=''$CONDA_INSTALL_PACKAGES' "'$CONDA_ENV_PROFILE_PARSED_PACKAGE'",'
	done
	CONDA_INSTALL_PACKAGES=${CONDA_INSTALL_PACKAGES::-1}
	echo '{ "channel": "file://'$ANACONDA_LOCAL_CHANNEL_DIR'", "packages": ['$CONDA_INSTALL_PACKAGES' ] }' > $CONDA_ENV_PACKAGES_LIST_FILE
	eval $__resultvar="'$CONDA_ENV_PACKAGES_LIST_FILE'"
}

parseCondaEnvProfile() {
	local CONDA_ENV_PROFILE_TEMPLATE=$1
	local CONDA_ENV_PROFILE_SECTION="none"
	export CONDA_ENV_PROFILE_PARSED_NAME=""
	export CONDA_ENV_PROFILE_PARSED_CHANNELS=""
	export CONDA_ENV_PROFILE_PARSED_PACKAGES=""
	while IFS= read -r line
	do
		if [[ $line == name* ]]
		then
			trim ${line#name:} CONDA_ENV_PROFILE_PARSED_NAME
		elif [[ $line == "channels:" ]]
		then
			CONDA_ENV_PROFILE_SECTION="channels"
		elif [[ "$line" == "dependencies:" ]]
		then
			CONDA_ENV_PROFILE_SECTION="dependencies"
		elif [[ "$CONDA_ENV_PROFILE_SECTION" == "channels" ]]
		then
			trim "$line" CONDA_ENV_PROFILE_PARSED_CHANNEL
			trim "${CONDA_ENV_PROFILE_PARSED_CHANNEL#?}" CONDA_ENV_PROFILE_PARSED_CHANNEL
			CONDA_ENV_PROFILE_PARSED_CHANNELS="$CONDA_ENV_PROFILE_PARSED_CHANNELS $CONDA_ENV_PROFILE_PARSED_CHANNEL"
		elif [[ "$CONDA_ENV_PROFILE_SECTION" == "dependencies" ]]
		then
			trim "$line" CONDA_ENV_PROFILE_PARSED_PACKAGE
			trim "${CONDA_ENV_PROFILE_PARSED_PACKAGE#?}" CONDA_ENV_PROFILE_PARSED_PACKAGE
			CONDA_ENV_PROFILE_PARSED_PACKAGES="$CONDA_ENV_PROFILE_PARSED_PACKAGES $CONDA_ENV_PROFILE_PARSED_PACKAGE"
		fi
	done < "$CONDA_ENV_PROFILE_TEMPLATE"
	trim "$CONDA_ENV_PROFILE_PARSED_CHANNELS" CONDA_ENV_PROFILE_PARSED_CHANNELS
	trim "$CONDA_ENV_PROFILE_PARSED_PACKAGES" CONDA_ENV_PROFILE_PARSED_PACKAGES
}

convertCondaTemplateToProfile() {
	local CONDA_ENV_PROFILE_TEMPLATE=$1
	local CONDA_ENV_NAME=$2
	local __resultvar=$3
	CONDA_ENV_PROFILE=/tmp/condaEnv-profile-`date +%s%N | md5sum | head -c8`.yaml
	cp $CONDA_ENV_PROFILE_TEMPLATE $CONDA_ENV_PROFILE 2>&1 | tee -a $LOG_FILE
	sed -i 's/##CONDA_ENV_NAME##/'$CONDA_ENV_NAME'/g' $CONDA_ENV_PROFILE 2>&1 | tee -a $LOG_FILE
	eval $__resultvar="'$CONDA_ENV_PROFILE'"
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

getCondaEnvs(){
	local ANACONDA_INSTANCE_ID=$1
	getRestUrls
	local CURL_OUT=`curl -s -S -k -w "%{http_code}" -u $EGO_ADMIN_USERNAME:$EGO_ADMIN_PASSWORD -X GET -H 'Accept:application/json' ${ASCD_REST_BASE_URL}conductor/v1/anaconda/instances/$ANACONDA_INSTANCE_ID/environments`
	local RESPONSE=${CURL_OUT:0:(-3)}
	local HTTP_CODE=${CURL_OUT:(-3)}
	if [ "$HTTP_CODE" == "200" ]
	then
		export CONDA_ENVIRONMENTS=$RESPONSE
	else
		log "Failed to retrieve conda environments of Anaconda instance $ANACONDA_INSTANCE_ID (HTTP CODE $HTTP_CODE), aborting. Output of REST call:" ERROR
		log "$RESPONSE" ERROR
		exit 1
	fi
}

getCondaEnv(){
	local ANACONDA_INSTANCE_ID=$1
	local CONDA_ENV_NAME=$2
	getRestUrls
	local CURL_OUT=`curl -s -S -k -w "%{http_code}" -u $EGO_ADMIN_USERNAME:$EGO_ADMIN_PASSWORD -X GET -H 'Accept:application/json' ${ASCD_REST_BASE_URL}conductor/v1/anaconda/instances/$ANACONDA_INSTANCE_ID/environments/$CONDA_ENV_NAME`
	local RESPONSE=${CURL_OUT:0:(-3)}
	local HTTP_CODE=${CURL_OUT:(-3)}
	if [ "$HTTP_CODE" == "200" ]
	then
		export CONDA_ENVIRONMENT_DETAILS=$RESPONSE
	else
		log "Failed to retrieve conda environment $CONDA_ENV_NAME of Anaconda instance $ANACONDA_INSTANCE_ID (HTTP CODE $HTTP_CODE), aborting. Output of REST call:" ERROR
		log "$RESPONSE" ERROR
		exit 1
	fi
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
		log "Anaconda distribution $ANACONDA_DISTRIBUTION_ID delete operation started successfully" SUCCESS
	else
		log "Failed to start deletion of Anaconda distribution $ANACONDA_DISTRIBUTION_ID (HTTP CODE $HTTP_CODE). Output of deletion tentative:" WARNING
		log "$RESPONSE" WARNING
	fi
}

deleteAnacondaInstance() {
	local ANACONDA_INSTANCE_ID=$1
	getRestUrls
	local CURL_OUT=`curl -s -S -k -w "%{http_code}" -u $EGO_ADMIN_USERNAME:$EGO_ADMIN_PASSWORD -H "Accept: application/json" -X DELETE ${ASCD_REST_BASE_URL}conductor/v1/anaconda/instances/$ANACONDA_INSTANCE_ID`
	local RESPONSE=${CURL_OUT:0:(-3)}
	local HTTP_CODE=${CURL_OUT:(-3)}
	if [ "$HTTP_CODE" == "204" ]
	then
		log "Anaconda instance $ANACONDA_INSTANCE_ID delete operation started successfully" SUCCESS
	else
		log "Failed to start deletion of Anaconda instance $ANACONDA_INSTANCE_ID (HTTP CODE $HTTP_CODE). Output of deletion tentative:" WARNING
		log "$RESPONSE" WARNING
	fi
}

getAnacondaDistributionExistence(){
	local ANACONDA_DISTRIBUTION_ID=$1
	getRestUrls
	local CURL_OUT=`curl -s -S -k -w "%{http_code}" -u $EGO_ADMIN_USERNAME:$EGO_ADMIN_PASSWORD -X GET ${ASCD_REST_BASE_URL}conductor/v1/anaconda/distributions/$ANACONDA_DISTRIBUTION_ID`
	local RESPONSE=${CURL_OUT:0:(-3)}
	local HTTP_CODE=${CURL_OUT:(-3)}
	if [ "$HTTP_CODE" == "200" ]
	then
		export ANACONDA_DISTRIBUTION_EXIST=true
	else
		export ANACONDA_DISTRIBUTION_EXIST=false
	fi
}

getAnacondaInstanceExistence(){
	local ANACONDA_INSTANCE_NAME=$1
	export ANACONDA_INSTANCE_EXIST=false
	getRestUrls
	local CURL_OUT=`curl -s -S -k -w "%{http_code}" -u $EGO_ADMIN_USERNAME:$EGO_ADMIN_PASSWORD -X GET ${ASCD_REST_BASE_URL}conductor/v1/anaconda/instances/`
	local RESPONSE=${CURL_OUT:0:(-3)}
	local HTTP_CODE=${CURL_OUT:(-3)}
	if [ "$HTTP_CODE" == "200" ]
	then
		echo $RESPONSE | $PYTHON_BIN -c $'import sys,json\nais=json.load(sys.stdin)\nfor ai in ais:\n  if (ai["name"]=="'$ANACONDA_INSTANCE_NAME'"): print "exists"' | grep "exists" >/dev/null 2>&1
		if [ $? -eq 0 ]
		then
			export ANACONDA_INSTANCE_EXIST=true
		fi
	fi
}

getCondaEnvExistence(){
	local ANACONDA_INSTANCE_UUID=$1
	local CONDA_ENV_NAME=$2
	getRestUrls
	local CURL_OUT=`curl -s -S -k -w "%{http_code}" -u $EGO_ADMIN_USERNAME:$EGO_ADMIN_PASSWORD -X GET ${ASCD_REST_BASE_URL}conductor/v1/anaconda/instances/$ANACONDA_INSTANCE_UUID/environments/$CONDA_ENV_NAME`
	local RESPONSE=${CURL_OUT:0:(-3)}
	local HTTP_CODE=${CURL_OUT:(-3)}
	if [ "$HTTP_CODE" == "200" ]
	then
		export CONDA_ENV_EXIST=true
	else
		export CONDA_ENV_EXIST=false
	fi
}
