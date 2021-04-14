#!/bin/sh

#############################
# WARNING: PLEASE READ README.md FIRST
#############################

source `dirname "$(readlink -f "$0")"`/conf/parameters.inc
source `dirname "$(readlink -f "$0")"`/conf/lab-environment.inc
source `dirname "$(readlink -f "$0")"`/functions/functions.inc
export LOG_FILE=$LOG_DIR/prepare-local-conda-channel_`hostname -s`.log
[[ ! -d $LOG_DIR ]] && mkdir -p $LOG_DIR && chmod 777 $LOG_DIR

log "Starting prepare local conda channel script"

[[ ! -d $CACHE_DIR ]] && prepareDir $CACHE_DIR $CLUSTERADMIN

if [ "$ANACONDA_DISTRIBUTION_NAME_TO_ADD" != "" ]
then
  downloadAnacondaDistribution $ANACONDA_DISTRIBUTION_NAME_TO_ADD "$CACHE_DIR/${ANACONDA_DISTRIBUTION_NAME_TO_ADD}.sh"
  INFO_MESSAGE_1="  - Anaconda distribution: $CACHE_DIR/${ANACONDA_DISTRIBUTION_NAME_TO_ADD}.sh"
fi

if [ "$ANACONDA_LOCAL_CHANNEL" == "enabled" ]
then
  log "Extracting WMLA-DL archive if necessary"
  extractTarArchive $CONDA_OPENCE_ARCHIVE $CONDA_OPENCE_DIR
  
  createLocalCondaChannel $ANACONDA_LOCAL_DISTRIBUTION_NAME $ANACONDA_LOCAL_INSTALL_DIR $ANACONDA_LOCAL_CHANNEL_DIR $ANACONDA_LOCAL_CHANNEL_ARCHIVE "$DLI_CONDA_DLINSIGHTS_PROFILE_TEMPLATE $IG_SPARK301_CONDA_ENV_PROFILE_TEMPLATE $IG_DLI_CONDA_ENV_PROFILE_TEMPLATE"
  INFO_MESSAGE_2="  - Local conda channel archive: $ANACONDA_LOCAL_CHANNEL_ARCHIVE"
fi

log "The following files were prepared and need to be copied in the scripts folder (following the same structure) which will be used to install the cluster:"
if [ "$INFO_MESSAGE_1" != "" ]
then
  log "$INFO_MESSAGE_1" NODATE
fi
if [ "$INFO_MESSAGE_2" != "" ]
then
  log "$INFO_MESSAGE_2" NODATE
fi

log "Local conda channel preparation finished!" SUCCESS
