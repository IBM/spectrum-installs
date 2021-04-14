#!/bin/sh

log() {
  local COLOR_RED=`tput setaf 1`
  local COLOR_GREEN=`tput setaf 2`
  local COLOR_YELLOW=`tput setaf 3`
  local COLOR_RESET=`tput sgr0`

  if [ "$2" == "NODATE" ]
  then
    echo -e "$1"
    echo -e "$1" >> $LOG_FILE
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
    echo -e ${COLOR}`date "+%Y-%m-%d %H:%M:%S"` "$1"${COLOR_RESET}

    echo -e `date "+%Y-%m-%d %H:%M:%S"` - $LOG_TYPE - "$1" >> $LOG_FILE
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
  changeOwnershipDir $DIR_PATH $OWNER
}

changeOwnershipDir() {
  local DIR_PATH=$1
  local OWNER=$2
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
    if [[ "$SSH_PORT" != "" && "$SSH_PORT" != "22" ]]
    then
      SSH_PORT_ARG="-p $SSH_PORT"
    fi
    ssh -t $HOST_NAME $SSH_PORT_ARG -C "$COMMAND" 2>&1 | tee -a $LOG_FILE
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

trim() {
  local var="$1"
  local __resultvar=$2
  var="${var#"${var%%[![:space:]]*}"}"
  var="${var%"${var##*[![:space:]]}"}"
  eval $__resultvar="'$var'"
}

applyUlimits() {
  log "Apply ulimit of 65536 for open files and max user processes, for root and $CLUSTERADMIN"
  ulimit -n 65536 2>&1 | tee -a $LOG_FILE
  ulimit -u 65536 2>&1 | tee -a $LOG_FILE
  su -l $CLUSTERADMIN -c "ulimit -n 65536" 2>&1 | tee -a $LOG_FILE
  su -l $CLUSTERADMIN -c "ulimit -u 65536" 2>&1 | tee -a $LOG_FILE
}

checkPython() {
  which $PYTHON_BIN >/dev/null 2>&1
  if [ $? -ne 0 ]
  then
    log "python binary ($PYTHON_BIN) not found" ERROR
    exit 1
  else
    $PYTHON_BIN --version 2>&1 | grep "Python 2.7" > /dev/null
    if [ $? -ne 0 ]
    then
      log "python binary ($PYTHON_BIN) is not good version (2.7.x required)" ERROR
      exit 1
    fi
  fi
}

checkJq() {
  which $JQ_BIN >/dev/null 2>&1
  if [ $? -ne 0 ]
  then
    log "jq binary ($JQ_BIN) not found" ERROR
    exit 1
  fi
}

extractTarArchive(){
  local ARCHIVE_FILE=$1
  local ARCHIVE_EXTRACT_DIR=$2
  if [ -d "$ARCHIVE_EXTRACT_DIR" ]
  then
    log "Directory $ARCHIVE_EXTRACT_DIR already exists"
  else
    log "Creating directory $ARCHIVE_EXTRACT_DIR"
    mkdir -p $ARCHIVE_EXTRACT_DIR >/dev/null 2>&1
    local CODE=$?
    if [ $CODE -ne 0 ]
    then
      log "Error when creating directory $ARCHIVE_EXTRACT_DIR (error code of mkdir: $CODE)" WARNING
    else
      log "Extracting $ARCHIVE_FILE in $ARCHIVE_EXTRACT_DIR"
      tar xf $ARCHIVE_FILE -C $ARCHIVE_EXTRACT_DIR >/dev/null 2>&1
      local CODE=$?
      if [ $CODE -ne 0 ]
      then
        log "Error when creating directory $ARCHIVE_EXTRACT_DIR (error code of tar: $CODE)" WARNING
      else
        log "Archive successfully extracted" SUCCESS
      fi
    fi
  fi
}
