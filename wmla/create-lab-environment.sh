#!/bin/sh

source `dirname "$(readlink -f "$0")"`/conf/parameters.inc
source `dirname "$(readlink -f "$0")"`/conf/lab-environment.inc
source `dirname "$(readlink -f "$0")"`/functions/functions.inc
export LOG_FILE=$LOG_DIR/create-lab-environment_`hostname -s`.log
[[ ! -d $LOG_DIR ]] && mkdir -p $LOG_DIR && chmod 777 $LOG_DIR

[[ ! -f $IG_SPARK301_PROFILE_TEMPLATE ]] && log "Spark301 Instance Group profile template $IG_SPARK301_PROFILE_TEMPLATE doesn't exist, aborting" ERROR && exit 1
[[ ! -f $IG_SPARK301_CONDA_ENV_PROFILE_TEMPLATE ]] && log "Spark301 conda profile template $IG_SPARK301_CONDA_ENV_PROFILE_TEMPLATE doesn't exist, aborting" ERROR && exit 1
[[ ! -f $IG_DLI_PROFILE_TEMPLATE ]] && log "DLI Instance Group profile template $IG_DLI_PROFILE_TEMPLATE doesn't exist, aborting" ERROR && exit 1
[[ ! -f $IG_DLI_CONDA_ENV_PROFILE_TEMPLATE ]] && log "DLI conda profile template $IG_DLI_CONDA_ENV_PROFILE_TEMPLATE doesn't exist, aborting" ERROR && exit 1

checkPython
checkJq

log "Starting to create lab environment"

log "Wait for cluster to start and REST URLs to be accessible"
waitForClusterUp
waitForRestUrlsUp

log "Finding available user id"
findAvailableUsername $LAB_USER_BASE LAB_USER

if [ "$LAB_CREATE_OS_USER" == "enabled" ]
then
  [[ ! -d $SCRIPTS_TMP_DIR ]] && mkdir -p $SCRIPTS_TMP_DIR
  SCRIPT_CREATE_OS_USER=$SCRIPTS_TMP_DIR/lab-create-os-user.sh
  echo '#!/bin/sh
if id "'$LAB_USER'" &>/dev/null; then
  echo "Cannot create OS user '$LAB_USER', it already exists"
else
  useradd '$LAB_USER' -g '$CLUSTERADMIN'
  CODE=$?
  if [ $CODE -eq 0 ]; then
    echo "User '$LAB_USER' created successfully"
    exit 0
  else
    echo "Failed to create user '$LAB_USER' (error code: $CODE)"
    exit 1
  fi
fi' > $SCRIPT_CREATE_OS_USER
  chmod +x $SCRIPT_CREATE_OS_USER 2>&1 | tee -a $LOG_FILE

  log "Creating OS user $LAB_USER on master $MASTERHOST"
  runCommandLocalOrRemote $MASTERHOST $SCRIPT_CREATE_OS_USER "false"

  if [[ "$MANAGEMENTHOSTS_FILE" != "" && -f $MANAGEMENTHOSTS_FILE && `wc -l $MANAGEMENTHOSTS_FILE | awk '{print $1}'` -gt 0 ]]
  then
    for MANAGEMENT_HOST in `cat $MANAGEMENTHOSTS_FILE`
    do
      log "Creating OS user $LAB_USER on management host $MANAGEMENT_HOST"
      runCommandLocalOrRemote $MANAGEMENT_HOST $SCRIPT_CREATE_OS_USER "false"
    done
  fi

  if [[ "$COMPUTEHOSTS_FILE" != "" && -f $COMPUTEHOSTS_FILE && `wc -l $COMPUTEHOSTS_FILE | awk '{print $1}'` -gt 0 ]]
  then
    for COMPUTE_HOST in `cat $COMPUTEHOSTS_FILE`
    do
      log "Creating OS user $LAB_USER on compute host $COMPUTE_HOST"
      runCommandLocalOrRemote $COMPUTE_HOST $SCRIPT_CREATE_OS_USER "false"
    done
  fi

  LAB_USER_HOME=`eval echo "~$LAB_USER"`

fi

log "Creating user $LAB_USER : $LAB_PASSWORD"
createUser $LAB_USER $LAB_PASSWORD

if [ "$RG_GPU_NAME" != "" ]
then
  log "Verifying if there are some GPU hosts in the cluster"
  getResourceGroupNbHosts $RG_GPU_NAME RG_GPU_NB_HOST
  if [ "$RG_GPU_NB_HOST" == "0" ]
  then
    log "$RG_GPU_NAME doesn't have any host, therefore using $RG_CPU_NAME for all components of the Instance Groups"
    RG_GPU_NAME=$RG_CPU_NAME
  fi
else
  RG_GPU_NAME=$RG_CPU_NAME
fi

if [ "$LAB_CREATE_OS_USER" == "enabled" ]
then
  LAB_EXEC_USER=$LAB_USER
else
  LAB_EXEC_USER=$CLUSTERADMIN
fi

log "Creating top level consumer for $LAB_USER"
createConsumer /${LAB_USER} $LAB_EXEC_USER $RG_CPU_NAME $RG_GPU_NAME $LAB_USER

if [ "$IG_ANACONDA_DISTRIBUTION_ID" == "" ]
then
  IG_ANACONDA_DISTRIBUTION_ID=$ANACONDA_DISTRIBUTION_ID_DEFAULT
fi

export IG_ANACONDA_INSTANCE_NAME=Anaconda-$LAB_USER
export IG_ANACONDA_INSTANCE_DEPLOY_HOME=$ANACONDA_DIR/$IG_ANACONDA_INSTANCE_NAME # Directory where Anaconda instance for Instance Groups will be deployed.
log "Create Anaconda instance using distribution $IG_ANACONDA_DISTRIBUTION_ID"
createAnacondaInstance "$IG_ANACONDA_DISTRIBUTION_ID" "$IG_ANACONDA_INSTANCE_NAME" "$IG_ANACONDA_INSTANCE_DEPLOY_HOME" "$LAB_EXEC_USER" "$RG_CPU_NAME" "${LAB_USER}"

if [ "$ANACONDA_AIRGAP_INSTALL" == "enabled" ]
then
  log "Create $IG_SPARK301_CONDA_ENV_NAME and $IG_DLI_CONDA_ENV_NAME conda environments based on the airgap archive package and wait for successful deployment"
  export ANACONDA_AIRGAP_INSTALL_IG_ARCHIVE=${ANACONDA_AIRGAP_INSTALL_IG_ARCHIVE_BASENAME}-${LAB_USER: -1}.tgz
  extractCondaEnvironmentsFromArchive $ANACONDA_AIRGAP_INSTALL_IG_ARCHIVE $IG_ANACONDA_INSTANCE_DEPLOY_HOME/anaconda "$IG_SPARK301_CONDA_ENV_NAME $IG_DLI_CONDA_ENV_NAME" $LAB_EXEC_USER
  discoverCondaEnvironments $IG_ANACONDA_INSTANCE_DEPLOY_HOME/anaconda "$IG_SPARK301_CONDA_ENV_NAME $IG_DLI_CONDA_ENV_NAME" $ANACONDA_INSTANCE_UUID
else
  log "Create $IG_DLI_CONDA_ENV_NAME conda environment and wait for successful deployment"
  if [ "$ANACONDA_LOCAL_CHANNEL" == "enabled" ]
  then
    prepareLocalCondaChannel
    log "Anaconda local channel enabled, modifying $IG_DLI_CONDA_ENV_NAME conda env profile template $IG_DLI_CONDA_ENV_PROFILE_TEMPLATE to use local channel"
    modifyCondaEnvironmentProfileWithLocalChannel $IG_DLI_CONDA_ENV_PROFILE_TEMPLATE "CONDA_PROFILE_TEMPLATE"
  else
    CONDA_PROFILE_TEMPLATE=$IG_DLI_CONDA_ENV_PROFILE_TEMPLATE
  fi
  createCondaEnvironment $ANACONDA_INSTANCE_UUID $CONDA_PROFILE_TEMPLATE $IG_DLI_CONDA_ENV_NAME
  log "Create $IG_SPARK301_CONDA_ENV_NAME conda environment and wait for successful deployment"
  if [ "$ANACONDA_LOCAL_CHANNEL" == "enabled" ]
  then
    prepareLocalCondaChannel
    log "Anaconda local channel enabled, modifying $IG_SPARK301_CONDA_ENV_NAME conda env profile template $IG_SPARK301_CONDA_ENV_PROFILE_TEMPLATE to use local channel"
    modifyCondaEnvironmentProfileWithLocalChannel $IG_SPARK301_CONDA_ENV_PROFILE_TEMPLATE "CONDA_PROFILE_TEMPLATE"
  else
    CONDA_PROFILE_TEMPLATE=$IG_SPARK301_CONDA_ENV_PROFILE_TEMPLATE
  fi
  createCondaEnvironment $ANACONDA_INSTANCE_UUID $CONDA_PROFILE_TEMPLATE $IG_SPARK301_CONDA_ENV_NAME
fi

IG_DLI_NAME=${LAB_USER}-${IG_DLI_BASENAME}
log "Creating consumers for instance group $IG_DLI_NAME"
createIgConsumers /${LAB_USER}/${IG_DLI_NAME} $IG_DLI_NAME $LAB_EXEC_USER $RG_CPU_NAME $RG_GPU_NAME $LAB_USER
updateResourcePlanIgConsumers $IG_DLI_NAME

log "Create Instance Group $IG_DLI_NAME"
if [ "$INSTALL_TYPE" == "local" ]
then
  createIgDli "$IG_DLI_PROFILE_TEMPLATE" "/${LAB_USER}/${IG_DLI_NAME}" "$IG_DLI_NAME" "$LAB_EXEC_USER" "$IG_DIR" "$SPARKHA_DIR" "$SPARKHISTORY_DIR" "$RG_CPU_NAME" "$RG_GPU_NAME" "$IG_DLI_CONDA_ENV_NAME" "$DLI_SHARED_FS/conf" "$DLI_SHARED_FS/distrib_workload_config" "$IG_ANACONDA_INSTANCE_DEPLOY_HOME/anaconda" "true"
else
  createIgDli "$IG_DLI_PROFILE_TEMPLATE" "/${LAB_USER}/${IG_DLI_NAME}" "$IG_DLI_NAME" "$LAB_EXEC_USER" "$IG_DIR" "$SPARKHA_DIR" "$SPARKHISTORY_DIR" "$RG_CPU_NAME" "$RG_GPU_NAME" "$IG_DLI_CONDA_ENV_NAME" "$DLI_SHARED_FS/conf" "$DLI_SHARED_FS/distrib_workload_config" "$IG_ANACONDA_INSTANCE_DEPLOY_HOME/anaconda" "false" "$SPARKSHUFFLE_DIR"
fi

IG_DLIEDT_NAME=${LAB_USER}-${IG_DLIEDT_BASENAME}
log "Creating consumers for instance group $IG_DLIEDT_NAME"
createIgConsumers /${LAB_USER}/${IG_DLIEDT_NAME} $IG_DLIEDT_NAME $LAB_EXEC_USER $RG_CPU_NAME $RG_GPU_NAME $LAB_USER
updateResourcePlanIgConsumers $IG_DLIEDT_NAME

log "Create Instance Group $IG_DLIEDT_NAME"
if [ "$INSTALL_TYPE" == "local" ]
then
  createIgDliEdt "$IG_DLIEDT_PROFILE_TEMPLATE" "/${LAB_USER}/${IG_DLIEDT_NAME}" "$IG_DLIEDT_NAME" "$LAB_EXEC_USER" "$IG_DIR" "$SPARKHA_DIR" "$SPARKHISTORY_DIR" "$RG_CPU_NAME" "$RG_GPU_NAME" "$IG_DLI_CONDA_ENV_NAME" "$DLI_SHARED_FS/conf" "$IG_ANACONDA_INSTANCE_DEPLOY_HOME/anaconda" "true"
else
  createIgDliEdt "$IG_DLIEDT_PROFILE_TEMPLATE" "/${LAB_USER}/${IG_DLIEDT_NAME}" "$IG_DLIEDT_NAME" "$LAB_EXEC_USER" "$IG_DIR" "$SPARKHA_DIR" "$SPARKHISTORY_DIR" "$RG_CPU_NAME" "$RG_GPU_NAME" "$IG_DLI_CONDA_ENV_NAME" "$DLI_SHARED_FS/conf" "$IG_ANACONDA_INSTANCE_DEPLOY_HOME/anaconda" "false" "$SPARKSHUFFLE_DIR"
fi

IG_SPARK301_NAME=${LAB_USER}-${IG_SPARK301_BASENAME}
log "Creating consumers for instance group $IG_SPARK301_NAME"
createIgConsumers /${LAB_USER}/${IG_SPARK301_NAME} $IG_SPARK301_NAME $LAB_EXEC_USER $RG_CPU_NAME $RG_GPU_NAME $LAB_USER
updateResourcePlanIgConsumers $IG_SPARK301_NAME

log "Create Instance Group $IG_SPARK301_NAME"
if [ "$INSTALL_TYPE" == "local" ]
then
  createIgSparkJupyter "$IG_SPARK301_PROFILE_TEMPLATE" "/${LAB_USER}/${IG_SPARK301_NAME}" "$IG_SPARK301_NAME" "$LAB_EXEC_USER" "$IG_DIR" "$SPARKHA_DIR" "$SPARKHISTORY_DIR" "$NOTEBOOKS_DIR" "$RG_CPU_NAME" "$RG_GPU_NAME" "$IG_ANACONDA_INSTANCE_NAME" "$IG_SPARK301_CONDA_ENV_NAME" "true"
else
  createIgSparkJupyter "$IG_SPARK301_PROFILE_TEMPLATE" "/${LAB_USER}/${IG_SPARK301_NAME}" "$IG_SPARK301_NAME" "$LAB_EXEC_USER" "$IG_DIR" "$SPARKHA_DIR" "$SPARKHISTORY_DIR" "$NOTEBOOKS_DIR" "$RG_CPU_NAME" "$RG_GPU_NAME" "$IG_ANACONDA_INSTANCE_NAME" "$IG_SPARK301_CONDA_ENV_NAME" "false" "$SPARKSHUFFLE_DIR"
fi

log "Get UUID of Instance Group $IG_SPARK301_NAME"
getIgUUID $IG_SPARK301_NAME

log "Create Notebook Instance for user $LAB_USER on Instance Group $IG_SPARK301_NAME"
createIgNotebookInstance "$LAB_USER" "$IG_UUID" "Jupyter" "6.0.0"

if [ -d $NOTEBOOK_SOURCE_DIR ]
then
  log "Create sample notebooks for user $LAB_USER on Instance Group $IG_SPARK301_NAME using the source directory: $NOTEBOOK_SOURCE_DIR"
  getIgUUID $IG_SPARK301_NAME
  createSampleNotebooks "$LAB_USER" "$IG_UUID" "$LAB_EXEC_USER" "$NOTEBOOK_SOURCE_DIR"
fi

if [ -d $DLI_DATASET_SOURCE_DIR ]
then
  getRestUrlsDLPD
  DATASET_NAME=${DLI_DATASET_BASENAME}-${LAB_USER}-$(date +%s)
  log "Create new dataset $DATASET_NAME from input location $DLI_DATASET_SOURCE_DIR on instance group $IG_UUID"
  getIgUUID "$IG_DLI_NAME"
  if [ `stat -c "%G" $DLI_DATASET_SOURCE_DIR` != "$CLUSTERADMIN" ]
  then
    log "Changing ownership of $DLI_DATASET_SOURCE_DIR to $CLUSTERADMIN:$CLUSTERADMIN"
    chown -R $CLUSTERADMIN:$CLUSTERADMIN $DLI_DATASET_SOURCE_DIR 2>&1 | tee -a $LOG_FILE
  fi
  CURL_OUT=`curl -s -S -k -w "%{http_code}" -u ${LAB_USER}:${LAB_PASSWORD} -H 'Accept:application/json' -H 'Content-Type:application/json' --data '{"name":"'$DATASET_NAME'","dbbackend":"TFRecords","imagedetail":{"isusingtext":false,"imagetype":"Color","resizetransformation":"Squash","splitalgorithm":"hold-out","trainimagepath":"'$DLI_DATASET_SOURCE_DIR'","valpercentage":10,"testpercentage":20,"traintextpath":null,"valtextpath":null,"testtextpath":null,"labeltextpath":null,"height":null,"width":null},"sigid":"'$IG_UUID'","datasourcetype":"IMAGEFORCLASSIFICATION","byclass":true}' ${DLPD_REST_BASE_URL}deeplearning/v1/datasets`
  RESPONSE=${CURL_OUT:0:(-3)}
  HTTP_CODE=${CURL_OUT:(-3)}
  if [ "$HTTP_CODE" == "201" ]
  then
    log "Dataset creation started successfully" SUCCESS
  else
    log "Failed to create dataset (HTTP CODE: $HTTP_CODE), aborting. Output of dataset creation tentative:" ERROR
    log "$RESPONSE" ERROR
    exit 1
  fi

  IMPORT_DATASET_STATUS=
  while [ "$IMPORT_DATASET_STATUS" != "FINISHED" ];
  do
    IMPORT_DATASET_STATUS=$(curl -s -S -k -u ${LAB_USER}:${LAB_PASSWORD} -H 'Accept:application/json' -H 'Content-Type:application/json' ${DLPD_REST_BASE_URL}deeplearning/v1/datasets/${DATASET_NAME} | jq -r '.status' )
    log "Importing dataset status... $IMPORT_DATASET_STATUS"
    sleep 5
  done
fi

if [ -d $DLI_MODEL_SOURCE_DIR ]
then
  getRestUrlsDLPD
  MODEL_TEMPLATE_NAME=${DLI_MODEL_BASENAME}-${LAB_USER}
  log "Adding model template $MODEL_TEMPLATE_NAME"
  CURL_OUT=`curl -s -S -k -w "%{http_code}" -u ${LAB_USER}:${LAB_PASSWORD} -H 'Accept:application/json' -H 'Content-Type:application/json' --data '{"name":"'$MODEL_TEMPLATE_NAME'","framework":"PyTorch","path":"'$DLI_MODEL_SOURCE_DIR'","description":"resnet pytorch base model"}' ${DLPD_REST_BASE_URL}deeplearning/v1/modeltemplates`
  RESPONSE=${CURL_OUT:0:(-3)}
  HTTP_CODE=${CURL_OUT:(-3)}
  if [ "$HTTP_CODE" == "201" ]
  then
    log "Model template added successfully" SUCCESS
  else
    log "Failed to add model template (HTTP CODE: $HTTP_CODE), aborting. Output of model template creation tentative:" ERROR
    log "$RESPONSE" ERROR
    exit 1
  fi
fi

log "     Username: $LAB_USER     Pass: $LAB_PASSWORD" WARNING
log "Lab environment created successfully!" SUCCESS
