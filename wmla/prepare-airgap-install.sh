#!/bin/sh

#############################
# WARNING: PLEASE READ README.md FIRST
#############################

source `dirname "$(readlink -f "$0")"`/conf/parameters.inc
source `dirname "$(readlink -f "$0")"`/conf/lab-environment.inc
source `dirname "$(readlink -f "$0")"`/functions/functions.inc
export LOG_FILE=$LOG_DIR/prepare-airgap-install_`hostname -s`.log
[[ ! -d $LOG_DIR ]] && mkdir -p $LOG_DIR && chmod 777 $LOG_DIR

checkPython

log "Starting prepare airgap install script"

[[ ! -d $CACHE_DIR ]] && prepareDir $CACHE_DIR $CLUSTERADMIN

if [ "$ANACONDA_DISTRIBUTION_NAME_TO_ADD" != "" ]
then
  downloadAnacondaDistribution $ANACONDA_DISTRIBUTION_NAME_TO_ADD "$CACHE_DIR/${ANACONDA_DISTRIBUTION_NAME_TO_ADD}.sh"
  INFO_MESSAGE_1="  - Anaconda distribution: $CACHE_DIR/${ANACONDA_DISTRIBUTION_NAME_TO_ADD}.sh"
fi

if [ "$ANACONDA_AIRGAP_INSTALL" == "enabled" ]
then
  log "Extracting WMLA-DL archive if necessary"
  extractTarArchive $CONDA_OPENCE_ARCHIVE $CONDA_OPENCE_DIR

  convertCondaTemplateToProfile $DLI_CONDA_DLINSIGHTS_PROFILE_TEMPLATE $DLI_CONDA_DLINSIGHTS_NAME $CONDA_OPENCE_DIR DLI_CONDA_DLINSIGHTS_PROFILE
  createAirgapCondaInstall $DLI_ANACONDA_INSTANCE_DEPLOY_HOME/anaconda $DLI_CONDA_DLINSIGHTS_PROFILE $ANACONDA_AIRGAP_INSTALL_DLINSIGHTS_ARCHIVE
  rm -rf $DLI_ANACONDA_INSTANCE_DEPLOY_HOME 2>&1 | tee -a $LOG_FILE
  INFO_MESSAGE_2="  - Airgap conda install archive for dlinsights: $ANACONDA_AIRGAP_INSTALL_DLINSIGHTS_ARCHIVE"

  convertCondaTemplateToProfile $IG_SPARK301_CONDA_ENV_PROFILE_TEMPLATE $IG_SPARK301_CONDA_ENV_NAME $CONDA_OPENCE_DIR IG_SPARK301_CONDA_ENV_PROFILE
  convertCondaTemplateToProfile $IG_DLI_CONDA_ENV_PROFILE_TEMPLATE $IG_DLI_CONDA_ENV_NAME $CONDA_OPENCE_DIR IG_DLI_CONDA_ENV_PROFILE
  LAB_ENV=1
  while [ $LAB_ENV -le $ANACONDA_AIRGAP_NB_LAB_ENVS ]
  do
    export IG_ANACONDA_INSTANCE_DEPLOY_HOME=$ANACONDA_DIR/Anaconda-${LAB_USER_BASE}${LAB_ENV}
    export ANACONDA_AIRGAP_INSTALL_IG_ARCHIVE=${ANACONDA_AIRGAP_INSTALL_IG_ARCHIVE_BASENAME}-${LAB_ENV}.tgz
    createAirgapCondaInstall $IG_ANACONDA_INSTANCE_DEPLOY_HOME/anaconda "$IG_SPARK301_CONDA_ENV_PROFILE $IG_DLI_CONDA_ENV_PROFILE" $ANACONDA_AIRGAP_INSTALL_IG_ARCHIVE
    rm -rf $IG_ANACONDA_INSTANCE_DEPLOY_HOME 2>&1 | tee -a $LOG_FILE
    INFO_MESSAGE_3="${INFO_MESSAGE_3}  - Airgap conda install archive for instance groups of lab environment $LAB_ENV: $ANACONDA_AIRGAP_INSTALL_IG_ARCHIVE\n"
    LAB_ENV=$((LAB_ENV+1))
  done
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
if [ "$INFO_MESSAGE_3" != "" ]
then
  log "${INFO_MESSAGE_3::-2}" NODATE
fi

log "Airgap install preparation finished!" SUCCESS
