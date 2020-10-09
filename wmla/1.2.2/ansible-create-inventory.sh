#!/bin/sh

#############################
# WARNING: PLEASE READ README.md FIRST
#############################

source `dirname "$(readlink -f "$0")"`/conf/parameters.inc
source `dirname "$(readlink -f "$0")"`/functions/functions-common.inc
export LOG_FILE=$LOG_DIR/ansible-create-inventory_`hostname -s`.log
[[ ! -d $LOG_DIR ]] && mkdir -p $LOG_DIR && chmod 777 $LOG_DIR

log "Starting to create ansible inventory file"

if [ -f $ANSIBLE_INVENTORY_FILE ]
then
  BACKUP_FILE=$ANSIBLE_INVENTORY_FILE.backup-`date "+%Y-%m-%d_%H-%M-%S"`
  log "Backup $ANSIBLE_INVENTORY_FILE to $BACKUP_FILE"
  \mv -f $ANSIBLE_INVENTORY_FILE $BACKUP_FILE 2>&1 | tee -a $LOG_FILE
fi

log "Adding variables"
echo "[all:vars]" > $ANSIBLE_INVENTORY_FILE
echo "scripts_dir=\"`dirname "$(readlink -f "$0")"`\"" >> $ANSIBLE_INVENTORY_FILE
echo "install_dir=\"$INSTALL_DIR\"" >> $ANSIBLE_INVENTORY_FILE
echo "base_shared_dir=\"$BASE_SHARED_DIR\"" >> $ANSIBLE_INVENTORY_FILE
echo "ego_shared_dir=\"$EGO_SHARED_DIR\"" >> $ANSIBLE_INVENTORY_FILE
echo "cluster_admin=\"$CLUSTERADMIN\"" >> $ANSIBLE_INVENTORY_FILE
echo "install_type=\"$INSTALL_TYPE\"" >> $ANSIBLE_INVENTORY_FILE
echo "update_ssl=\"$CLUSTERINSTALL_UPDATE_SSL\"" >> $ANSIBLE_INVENTORY_FILE
echo "create_user_environment=\"$CLUSTERINSTALL_CREATE_USER_ENVIRONMENT\"" >> $ANSIBLE_INVENTORY_FILE
echo "" >> $ANSIBLE_INVENTORY_FILE

log "Adding masterhost group"
echo "[masterhost]" >> $ANSIBLE_INVENTORY_FILE
echo "$MASTERHOST" >> $ANSIBLE_INVENTORY_FILE
echo "" >> $ANSIBLE_INVENTORY_FILE

log "Adding managementhosts group"
echo "[managementhosts]" >> $ANSIBLE_INVENTORY_FILE
cat $MANAGEMENTHOSTS_FILE >> $ANSIBLE_INVENTORY_FILE
echo "" >> $ANSIBLE_INVENTORY_FILE

if [ "$MASTER_CANDIDATES" != "" ]
then
  log "Adding mastercandidatehosts group"
  echo "[mastercandidatehosts]" >> $ANSIBLE_INVENTORY_FILE
  for MASTER_CANDIDATE in ${MASTER_CANDIDATES//,/ }
  do
    echo $MASTER_CANDIDATE >> $ANSIBLE_INVENTORY_FILE
  done
  echo "" >> $ANSIBLE_INVENTORY_FILE
fi

log "Adding computehosts group"
echo "[computehosts]" >> $ANSIBLE_INVENTORY_FILE
cat $COMPUTEHOSTS_FILE >> $ANSIBLE_INVENTORY_FILE
echo "" >> $ANSIBLE_INVENTORY_FILE

log "Ansible inventory file $ANSIBLE_INVENTORY_FILE created" SUCCESS
log "To install the cluster using the Ansible playbook, run the following command:"
log "ansible-playbook ansible-install-cluster.yaml -i $ANSIBLE_INVENTORY_FILE" NODATE
