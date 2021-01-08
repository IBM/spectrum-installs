#!/bin/sh

#############################
# WARNING: PLEASE READ README.md FIRST
#############################

source `dirname "$(readlink -f "$0")"`/conf/parameters.inc
source `dirname "$(readlink -f "$0")"`/functions/functions.inc
export LOG_FILE=$LOG_DIR/prepare-airgap-install_`hostname -s`.log
[[ ! -d $LOG_DIR ]] && mkdir -p $LOG_DIR && chmod 777 $LOG_DIR

log "Starting prepare airgap install script"

[[ ! -d $CACHE_DIR ]] && prepareDir $CACHE_DIR $CLUSTERADMIN

if [ "$ANACONDA_DISTRIBUTION_NAME_TO_ADD" != "" ]
then
  downloadAnacondaDistribution $ANACONDA_DISTRIBUTION_NAME_TO_ADD "$CACHE_DIR/${ANACONDA_DISTRIBUTION_NAME_TO_ADD}.sh"
  INFO_MESSAGE_1="  - Anaconda distribution: $CACHE_DIR/${ANACONDA_DISTRIBUTION_NAME_TO_ADD}.sh"
fi

if [ "$ANACONDA_AIRGAP_INSTALL" == "enabled" ]
then
  convertCondaTemplateToProfile $IG_SPARK243_CONDA_ENV_PROFILE_TEMPLATE $IG_SPARK243_CONDA_ENV_NAME IG_SPARK243_CONDA_ENV_PROFILE
  createAirgapCondaInstall $IG_ANACONDA_INSTANCE_DEPLOY_HOME/anaconda "$IG_SPARK243_CONDA_ENV_PROFILE" $ANACONDA_AIRGAP_INSTALL_IG_ARCHIVE
  rm -rf $IG_ANACONDA_INSTANCE_DEPLOY_HOME 2>&1 | tee -a $LOG_FILE
  INFO_MESSAGE_2="  - Airgap conda install archive for instance groups: $ANACONDA_AIRGAP_INSTALL_IG_ARCHIVE"
fi

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
