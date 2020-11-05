#!/bin/sh

source `dirname "$(readlink -f "$0")"`/conf/parameters.inc
source `dirname "$(readlink -f "$0")"`/conf/lab-environment.inc
source `dirname "$(readlink -f "$0")"`/functions/functions.inc
export LOG_FILE=$LOG_DIR/delete-lab-env_`hostname -s`.log
[[ ! -d $LOG_DIR ]] && mkdir -p $LOG_DIR && chmod 777 $LOG_DIR

removeAnacondaInstance() {
  getRestUrls
  CURLBIN="curl -s -S -k -u $EGO_ADMIN_USERNAME:$EGO_ADMIN_PASSWORD -H 'Accept:application/json' -H 'Content-Type:application/json'"
  local ANACONDA_INSTANCE_NAME=$1
  local URL="$ASCD_REST_BASE_URL/anaconda/instances"
  local ANACONDA_INSTANCE_UUID=`curl -s -S -k -u $EGO_ADMIN_USERNAME:$EGO_ADMIN_PASSWORD -H 'Accept:application/json' -H 'Content-Type:application/json' -X GET ${ASCD_REST_BASE_URL}conductor/v1/anaconda/instances | jq -r '.[] | select(.name == "'$ANACONDA_INSTANCE_NAME'") | .id.uuid'`
  local ANACONDA_INSTANCE_DEPLOY_DIR=`curl -s -S -k -u $EGO_ADMIN_USERNAME:$EGO_ADMIN_PASSWORD -H 'Accept:application/json' -H 'Content-Type:application/json' -X GET ${ASCD_REST_BASE_URL}conductor/v1/anaconda/instances | jq -r '.[] | select(.name == "'$ANACONDA_INSTANCE_NAME'") | .servicepackages[].environments."ANACONDA_DEPLOY_DIR"'`
  local ANACONDA_CONSUMER_PATH_AND_NAME=`curl -s -S -k -u $EGO_ADMIN_USERNAME:$EGO_ADMIN_PASSWORD -H 'Accept:application/json' -H 'Content-Type:application/json' -X GET ${ASCD_REST_BASE_URL}conductor/v1/anaconda/instances | jq -r '.[] | select(.name == "'$ANACONDA_INSTANCE_NAME'") | .packages[]'` 
  if [ "$ANACONDA_INSTANCE_UUID" = "" ]; then
    log "Anaconda instance UUID not found in $ANACONDA_INSTANCE_NAME" WARNING
  else
    log "Removing Anaconda Instance: $ANACONDA_INSTANCE_UUID" 
    $CURLBIN -X DELETE ${ASCD_REST_BASE_URL}conductor/v1/anaconda/instances/$ANACONDA_INSTANCE_UUID?undeploy=true
    getAnacondaInstanceState $ANACONDA_INSTANCE_UUID
    while [ "$ANACONDA_INSTANCE_STATE" != "UNREGISTERING" ]
    do 
      log "Anaconda instance deploying (state: $ANACONDA_INSTANCE_STATE) ..."
      sleep $STATUS_CHECK_WAITTIME
      getAnacondaInstanceState $ANACONDA_INSTANCE_UUID
    done
    log "Removing Anaconda Service Package at: $ANACONDA_CONSUMER_PATH_AND_NAME" 
    $CURLBIN -X DELETE ${EGO_REST_BASE_URL}/platform/rest/deployment/v1/$ANACONDA_CONSUMER_PATH_AND_NAME > /dev/null

    if [ "${ANACONDA_INSTANCE_DEPLOY_DIR}x" != "x" ] && [ -d "$ANACONDA_INSTANCE_DEPLOY_DIR" ]; then
      log "Cleaning up anaconda instance directory $ANACONDA_INSTANCE_DEPLOY_DIR"
      rm -rf $ANACONDA_INSTANCE_DEPLOY_DIR
      log "Removal of anaconda instance directory complete." SUCCESS
    fi

  fi
}

log "Starting delete lab environment"

if [ "$1" == "" ]
then
  log "Username of the lab to delete must be passed as argument" ERROR
  exit 1
fi

LAB_USER=$1
log "Deleting lab environment of user $LAB_USER"

log "Wait for cluster to start"
waitForClusterUp
getRestUrls
getRestUrlsDLPD

log "Locating assets to remove: models"
MODEL_NAMES=$(curl -s -S -k -u $EGO_ADMIN_USERNAME:$EGO_ADMIN_PASSWORD -H 'Accept:application/json' -H 'Content-Type:application/json' ${DLPD_REST_BASE_URL}deeplearning/v1/models | jq -r '.[] | select(.user == "'${LAB_USER}'") | .name')
for MODEL_NAME in $MODEL_NAMES
do
  log "Removing model: $MODEL_NAME"
  curl -X DELETE -s -S -k -u $EGO_ADMIN_USERNAME:$EGO_ADMIN_PASSWORD -H 'Accept:application/json' -H 'Content-Type:application/json' ${DLPD_REST_BASE_URL}deeplearning/v1/models/${MODEL_NAME}
done

log "Locating assets to remove: model templates"
MODEL_TEMPLATE_NAMES=$(curl -s -S -k -u $EGO_ADMIN_USERNAME:$EGO_ADMIN_PASSWORD -H 'Accept:application/json' -H 'Content-Type:application/json' ${DLPD_REST_BASE_URL}deeplearning/v1/modeltemplates | jq -r '.[] | select(.creator == "'${LAB_USER}'") | .name')
for MODEL_TEMPLATE_NAME in $MODEL_TEMPLATE_NAMES
do
  log "Removing template: $MODEL_TEMPLATE_NAME"
  curl -X DELETE -s -S -k -u $EGO_ADMIN_USERNAME:$EGO_ADMIN_PASSWORD -H 'Accept:application/json' -H 'Content-Type:application/json' ${DLPD_REST_BASE_URL}deeplearning/v1/modeltemplates/${MODEL_TEMPLATE_NAME}
done

log "Locating assets to remove: datasets"
DATASET_NAMES=$(curl -s -S -k -u $EGO_ADMIN_USERNAME:$EGO_ADMIN_PASSWORD -H 'Accept:application/json' -H 'Content-Type:application/json' ${DLPD_REST_BASE_URL}deeplearning/v1/datasets | jq -r '.[] | select(.createUser == "'${LAB_USER}'") | .name')
for DATASET_NAME in $DATASET_NAMES
do
  log "Removing dataset: $DATASET_NAME"
  curl -X DELETE -s -S -k -u $EGO_ADMIN_USERNAME:$EGO_ADMIN_PASSWORD -H 'Accept:application/json' -H 'Content-Type:application/json' ${DLPD_REST_BASE_URL}deeplearning/v1/datasets/$DATASET_NAME
done

log "Locating IGs to remove"
IGS=$(curl -s -S -k -u $EGO_ADMIN_USERNAME:$EGO_ADMIN_PASSWORD -H 'Accept:application/json' -H 'Content-Type:application/json' -G ${ASCD_REST_BASE_URL}conductor/v1/instances -d consumerpath=/${LAB_USER} | jq -r '.[].name')
for IG in $IGS
do
  log "Deleting instance group $IG"
  removeIg $IG
done

log "Removing Anaconda Instance Anaconda-${LAB_USER}"
removeAnacondaInstance Anaconda-${LAB_USER}

log "Deleting consumer /$LAB_USER"
deleteConsumer $LAB_USER

log "Deleting user $LAB_USER"
deleteUser $LAB_USER

if [ "$LAB_CREATE_OS_USER" == "enabled" ]
then
  [[ ! -d $SCRIPTS_TMP_DIR ]] && mkdir -p $SCRIPTS_TMP_DIR
  SCRIPT_DELETE_OS_USER=$SCRIPTS_TMP_DIR/lab-delete-os-user.sh
  echo '#!/bin/sh
if id "'$LAB_USER'" &>/dev/null; then
  userdel -r '$LAB_USER'
  CODE=$?
  if [ $CODE -eq 0 ]; then
    echo "User '$LAB_USER' deleted successfully"
    exit 0
  else
    echo "Failed to delete user '$LAB_USER' (error code: $CODE)"
    exit 1
  fi
fi' > $SCRIPT_DELETE_OS_USER
  chmod +x $SCRIPT_DELETE_OS_USER 2>&1 | tee -a $LOG_FILE

  log "Deleting OS user $LAB_USER on master $MASTERHOST"
  runCommandLocalOrRemote $MASTERHOST $SCRIPT_DELETE_OS_USER "false"

  if [[ "$MANAGEMENTHOSTS_FILE" != "" && -f $MANAGEMENTHOSTS_FILE && `wc -l $MANAGEMENTHOSTS_FILE | awk '{print $1}'` -gt 0 ]]
  then
    for MANAGEMENT_HOST in `cat $MANAGEMENTHOSTS_FILE`
    do
      log "Deleting OS user $LAB_USER on management host $MANAGEMENT_HOST"
      runCommandLocalOrRemote $MANAGEMENT_HOST $SCRIPT_DELETE_OS_USER "false"
    done
  fi

  if [[ "$COMPUTEHOSTS_FILE" != "" && -f $COMPUTEHOSTS_FILE && `wc -l $COMPUTEHOSTS_FILE | awk '{print $1}'` -gt 0 ]]
  then
    for COMPUTE_HOST in `cat $COMPUTEHOSTS_FILE`
    do
      log "Deleting OS user $LAB_USER on compute host $COMPUTE_HOST"
      runCommandLocalOrRemote $COMPUTE_HOST $SCRIPT_DELETE_OS_USER "false"
    done
  fi
fi
 
log "Lab environment of user $LAB_USER deleted successfully!" SUCCESS
