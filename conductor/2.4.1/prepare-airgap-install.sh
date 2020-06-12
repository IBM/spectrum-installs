#!/bin/sh

#############################
# WARNING: PLEASE READ README.md FIRST
#############################

source `dirname "$(readlink -f "$0")"`/conf/parameters.inc
source `dirname "$(readlink -f "$0")"`/functions/functions.inc
export LOG_FILE=$LOG_DIR/prepare-airgap-install_`hostname -s`.log
[[ ! -d $LOG_DIR ]] && mkdir -p $LOG_DIR && chown $CLUSTERADMIN:$CLUSTERADMIN $LOG_DIR

log "Starting prepare airgap install script"

[[ ! -d $CACHE_DIR ]] && prepareDir $CACHE_DIR $CLUSTERADMIN

if [ "$ANACONDA_DISTRIBUTION_NAME_TO_ADD" != "" ]
then
  log "Downloading Anaconda distribution $ANACONDA_DISTRIBUTION_NAME_TO_ADD"
  downloadAnacondaDistribution $ANACONDA_DISTRIBUTION_NAME_TO_ADD "$CACHE_DIR/${ANACONDA_DISTRIBUTION_NAME_TO_ADD}.sh"
  INFO_MESSAGE_1="  - Anaconda distribution: $CACHE_DIR/${ANACONDA_DISTRIBUTION_NAME_TO_ADD}.sh"
fi

createLocalCondaChannel $ANACONDA_LOCAL_DISTRIBUTION_NAME $ANACONDA_LOCAL_INSTALL_DIR $ANACONDA_LOCAL_CHANNEL_DIR $ANACONDA_LOCAL_CHANNEL_ARCHIVE "$IG_SPARK243_CONDA_ENV_PROFILE_TEMPLATE"
INFO_MESSAGE_2="  - Local conda channel archive: $ANACONDA_LOCAL_CHANNEL_ARCHIVE"

log "The following files were prepared and need to be copied in the scripts folder (following the same structure) which will be used to install the cluster:"
if [ "$INFO_MESSAGE_1" != "" ]
then
  log "$INFO_MESSAGE_1"
fi
if [ "$INFO_MESSAGE_2" != "" ]
then
  log "$INFO_MESSAGE_2"
fi

log "Airgap install preparation finished!" SUCCESS
