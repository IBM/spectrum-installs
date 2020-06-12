#!/bin/sh

#############################
# WARNING: PLEASE READ README.md FIRST
#############################

source `dirname "$(readlink -f "$0")"`/conf/parameters.inc
source `dirname "$(readlink -f "$0")"`/functions/functions.inc
export LOG_FILE=$LOG_DIR/create-user-environment_`hostname -s`.log
[[ ! -d $LOG_DIR ]] && mkdir -p $LOG_DIR && chown $CLUSTERADMIN:$CLUSTERADMIN $LOG_DIR

[[ ! -f $IG_SPARK243_PROFILE_TEMPLATE ]] && log "Spark243 Instance Group profile template $IG_SPARK243_PROFILE_TEMPLATE doesn't exist, aborting" ERROR && exit 1
[[ ! -f $IG_SPARK243_CONDA_ENV_PROFILE_TEMPLATE ]] && log "Spark243 conda profile template $IG_SPARK243_CONDA_ENV_PROFILE_TEMPLATE doesn't exist, aborting" ERROR && exit 1

log "Starting to create user environment"

log "Wait for cluster to start and REST URLs to be accessible"
waitForClusterUp
waitForRestUrlsUp

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

if [ "$IG_ANACONDA_DISTRIBUTION_ID" == "" ]
then
	IG_ANACONDA_DISTRIBUTION_ID=$ANACONDA_DISTRIBUTION_ID_DEFAULT
fi
log "Create Anaconda instance using distribution $IG_ANACONDA_DISTRIBUTION_ID"
createAnacondaInstance "$IG_ANACONDA_DISTRIBUTION_ID" "$IG_ANACONDA_INSTANCE_NAME" "$IG_ANACONDA_INSTANCE_DEPLOY_HOME" "$CLUSTERADMIN" "$RG_CPU_NAME"

log "Creating user $IG_USER_NAME"
createUser $IG_USER_NAME $IG_USER_PASSWORD

log "Creating consumers for instance group $IG_SPARK243_NAME"
createIgConsumers $IG_SPARK243_NAME $CLUSTERADMIN $RG_CPU_NAME $RG_GPU_NAME $IG_USER_NAME
updateResourcePlanIgConsumers $IG_SPARK243_NAME

log "Create $IG_SPARK243_CONDA_ENV_NAME conda environment and wait for successful deployment"
if [ "$ANACONDA_LOCAL_CHANNEL" == "enabled" ]
then
	prepareLocalCondaChannel
	log "Anaconda local channel enabled, modifying $IG_SPARK243_CONDA_ENV_NAME conda env profile template $IG_SPARK243_CONDA_ENV_PROFILE_TEMPLATE to use local channel"
	modifyCondaEnvironmentProfileWithLocalChannel $IG_SPARK243_CONDA_ENV_PROFILE_TEMPLATE "CONDA_PROFILE_TEMPLATE"
else
	CONDA_PROFILE_TEMPLATE=$IG_SPARK243_CONDA_ENV_PROFILE_TEMPLATE
fi
createCondaEnvironmentAndWait $ANACONDA_INSTANCE_UUID $CONDA_PROFILE_TEMPLATE $IG_SPARK243_CONDA_ENV_NAME

log "Create Instance Group $IG_SPARK243_NAME"
if [ "$INSTALL_TYPE" == "local" ]
then
  createIgSparkJupyter "$IG_SPARK243_PROFILE_TEMPLATE" "/$IG_SPARK243_NAME" "$IG_SPARK243_NAME" "$CLUSTERADMIN" "$IG_DIR" "$SPARKHA_DIR" "$SPARKHISTORY_DIR" "$NOTEBOOKS_DIR" "$RG_CPU_NAME" "$RG_GPU_NAME" "$IG_ANACONDA_INSTANCE_NAME" "$IG_SPARK243_CONDA_ENV_NAME" "true"
else
  createIgSparkJupyter "$IG_SPARK243_PROFILE_TEMPLATE" "/$IG_SPARK243_NAME" "$IG_SPARK243_NAME" "$CLUSTERADMIN" "$IG_DIR" "$SPARKHA_DIR" "$SPARKHISTORY_DIR" "$NOTEBOOKS_DIR" "$RG_CPU_NAME" "$RG_GPU_NAME" "$IG_ANACONDA_INSTANCE_NAME" "$IG_SPARK243_CONDA_ENV_NAME" "false" "$SPARKSHUFFLE_DIR"
fi

log "Get UUID of Instance Group $IG_SPARK243_NAME"
getIgUUID $IG_SPARK243_NAME

log "Create Notebook Instance for user $EGO_ADMIN_USERNAME on Instance Group $IG_SPARK243_NAME"
createIgNotebookInstance "$EGO_ADMIN_USERNAME" "$IG_UUID" "Jupyter" "5.4.0"
log "Create Notebook Instance for user $IG_USER_NAME on Instance Group $IG_SPARK243_NAME"
createIgNotebookInstance "$IG_USER_NAME" "$IG_UUID" "Jupyter" "5.4.0"

log "User environment created successfully!" SUCCESS
