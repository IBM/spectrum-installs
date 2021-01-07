#!/bin/sh

##### GLOBAL PARAMETERS
### CLUSTER INSTALL
export INSTALL_TYPE=local # Specify shared or local. Type of installation for Symphony binaries.
export CLUSTERADMIN=egoadmin # OS user who will be cluster admin.
export CLUSTERNAME=Symphony # Display name of the cluster.
export SSL=enabled # Specify enabled or disabled. If enabled, all web interfaces and REST services will use SSL.
export MASTERHOST=server1.domain.com # FQDN of master host of the cluster, as returned by "hostname -f".
export MASTER_CANDIDATES= # Specify all or a subset of management hosts which will be defined as master candidates. The list of hostnames (FQDN as returned by "hostname -f" must be separated with comma (",") and complete list enclosed with double quote). If this parameter is defined, EGO_SHARED_DIR must also be defined. Only used by install-cluster.sh.
export BASE_PORT=7869 # Optional, specify base port for Symphony components. The cluster uses seven consecutive ports from the base port.

### MULTI-HEAD INSTALL
export INSTALL_MULTI_HEAD=disabled # Specify enabled or disabled. If enabled, the scripts will install Symphony on an existing Conductor or WMLA cluster. Directories INSTALL_DIR, RPMDB_DIR and EGO_SHARED_DIR (if specified) must be the same than those used during Conductor / WMLA installation. EGO_ADMIN_USERNAME and EGO_ADMIN_PASSWORD also need to be updated with current credentials of Admin user.
export POSTINSTALL_AUTOMATIC_CONFIG=enabled # Specify enabled or disabled. If enabled and INSTALL_MULTI_HEAD=enabled, the post-install config will be done automatically instead of the wizard on the GUI the first time the Admin log in. Hadoop components will not be configured, so this setting should be disabled if Haddop configuration needs to be done.

### DEMO ENVIRONMENT
export CLUSTERINSTALL_CREATE_DEMO_ENVIRONMENT=disabled # Specify enabled or disabled. If enabled, script install-cluster.sh will create demo environment based on parameters below.

### HADOOP INTEGRATION
# Optional parameters to define if you are planning to use MapReduce framework and/or YARN integration on Symphony.
export INSTALL_MAPREDUCE=N # Specify Y to install MapReduce and YARN integration components, N to not install them.
export JAVA_HOME= # Specify path to Java directory to use for MapReduce framework.
export HADOOP_HOME= # Specify the installation directory of the Hadoop distribution.
export HADOOP_VERSION= # Specify the version of the Hadoop distribution.
export DFS_GUI_HOSTNAME= # Specify the host name of the HDFS NameNode.
export DFS_GUI_PORT= # Specify the port number for the HDFS NameNode.
export HADOOP_YARN_HOME= # Specify the installation directory for full distribution YARN.

##### DIRECTORIES PARAMETERS
### BASE DIRECTORIES
export BASE_INSTALL_DIR=/opt/ibm/spectrum # If INSTALL_TYPE=shared, this directory must be on shared FS.
export EGO_SHARED_DIR= # Optional, specify the EGO shared directory to use for High Availability (only useful if there are multiple management hosts). Must be on shared FS.

###### BELOW THIS POINT, YOU CAN KEEP DEFAULT CONFIGURATION UNLESS YOU WANT TO CUSTOMIZE IT #####

### INSTALL DIRECTORIES
export INSTALL_DIR=$BASE_INSTALL_DIR/install # Directory where Symphony will be installed. If INSTALL_TYPE=shared, it must be on shared FS.
export RPMDB_DIR=$BASE_INSTALL_DIR/rpmdb # Directory for Symphony RPMs database. If INSTALL_TYPE=shared, it must be on shared FS.

### SYMPHONY INSTALLER AND ENTITLEMENT
export SYMPHONY_BIN=`dirname "$(readlink -f "$0")"`/symphony/sym-7.3.1.0_`uname -m`.bin # Path to symphony 7.3.1 .bin installer.
export SYMPHONY_ENTITLEMENT=`dirname "$(readlink -f "$0")"`/symphony/sym_adv_entitlement.dat # Path to symphony 7.3.1 entitlement, either standard or advanced edition.

### DEMO ENVIRONMENT
export DEMO_USER=demo # Username of the user which will have access (consumer admin role) to the applications created.
export DEMO_PASSWORD=demo # Password of the user which will have access (consumer admin role) to the applications created.
export DEMO_APP_PROFILE_TEMPLATE=`dirname "$(readlink -f "$0")"`/templates/app.xml # Application profile template to use to create applications.
export DEMO_VAR_APP_NAME=Demo7.3.1 # Name of the Value at Risk demo application installed by Symphony
export DEMO_VAR_CONSUMER_PATH=/SampleApplications/SOADemo # Consumer in which DEMO_VAR_APP_NAME is deployed

##### SCRIPTS PARAMETERS
### WORKING DIRECTORIES
export CACHE_DIR=`dirname "$(readlink -f "$0")"`/cache # Base directory that these scripts will use to create temporary files.
export LOG_DIR=`dirname "$(readlink -f "$0")"`/logs # Directory where all log files will be written.
export SYNC_DIR=$CACHE_DIR/sync # Only required if INSTALL_TYPE=local and there are additional management hosts. Directory where install-host.sh script will write lock file to avoid conflict for the parallel execution of the script on different hosts.
export SCRIPTS_TMP_DIR=$CACHE_DIR/generated-scripts # Only required if you use install-cluster.sh. Directory where install-cluster.sh will generate scripts to install each type of host, and run them remotely.

### COMMON
export MANAGEMENTHOSTS_FILE=`dirname "$(readlink -f "$0")"`/conf/management-hosts.txt # Path of file containing list of additional management hosts (FQDN as returned by "hostname -f"), 1 host per line.
export COMPUTEHOSTS_FILE=`dirname "$(readlink -f "$0")"`/conf/compute-hosts.txt # Path of file containing list of compute hosts (FQDN as returned by "hostname -f"), 1 host per line. Only used by install-cluster.sh.
export PYTHON_BIN=python # Path to Python binary. Must be python 2.7.x.
export STATUS_CHECK_WAITTIME=5 # Duration in seconds to wait before 2 check of status.
export EGO_SHUTDOWN_WAITTIME=15 # Duration in seconds to wait after stopping or restarting EGO.

### INSTALLATION
export SSH_PORT=22 # Optional, specify the port for SSH connection. Only used by install-cluster.sh, forceuninstall-cluster.sh and create-lab-environment.sh if LAB_CREATE_OS_USER=enabled.
export PSSH_NBHOSTS_IN_PARALLEL=10 # Specify the number of management and compute hosts to install in parallel, only used by install-cluster.sh if pssh is installed and this parameter is greater than 1.
export PSSH_TIMEOUT=0 # Timeout in seconds for execution of the install on each host when using pssh. 0 means no timeout. Only used by install-cluster.sh if pssh is installed and parameter PSSH_NBHOSTS_IN_PARALLEL is greater than 1.
export INSTALL_FROM_RPMS=disabled # Specify enabled or disabled. If enabled, the script will extract rpms from Symphony bin installer and install from rpms with "--ignoresize" option (useful for Spectrum Scale shared install if you hit an error related to diskspace check).
export INSTALL_FROM_RPMS_TMP_DIR=$CACHE_DIR/rpms # Only required if INSTALL_FROM_RPMS=enabled. Specify directory where RPMs will be extracted.

### ANSIBLE
export ANSIBLE_INVENTORY_FILE=`dirname "$(readlink -f "$0")"`/ansible-inventory.ini # Path of Ansible inventory file that will be generated by ansible-create-inventory.sh and can be used by ansible-install-cluster.yaml and ansible-forceuninstall-cluster.yaml playbooks

### EGO DEFAULT SETTINGS
export EGO_ADMIN_USERNAME=Admin # Default Admin username after EGO installation, or current Admin username if INSTALL_MULTI_HEAD=enabled.
export EGO_ADMIN_PASSWORD=Admin # Default Admin password after EGO installation, or current Admin password if INSTALL_MULTI_HEAD=enabled.
export RG_MANAGEMENT_NAME=ManagementHosts # Default Resource Group with management hosts after EGO installation.
export RG_COMPUTE_NAME=ComputeHosts # Default Resource Group with compute hosts after EGO installation.
export RG_INTERNAL_NAME=InternalResourceGroup # Default Internal Resource Group after EGO installation.

### SSL
export SSL_TMP_DIR=$CACHE_DIR/ssl-files # Directory where SSL files will be written from the master host and need to be read by all other hosts. Need to be shared filesystem.
export SSL_CA_KEYSTORE_PASSWORD=Liberty # Default password of caKeyStore.jks keystore.
export SSL_TIER1_KEYSTORE_PASSWORD=Liberty # Default password of serverKeyStore.jks keystore.
