#!/bin/sh

##### GLOBAL PARAMETERS
### CLUSTER INSTALL
export INSTALL_TYPE=local # Specify shared or local. Type of installation for Symphony binaries.
export CLUSTERADMIN=egoadmin # OS user who will be cluster admin.
export CLUSTERNAME=Symphony # Display name of the cluster.
export SSL=enabled # Specify enabled or disabled. If enabled, all web interfaces and REST services will use SSL.
export MASTERHOST=server1.domain.com # FQDN of master host of the cluster, as returned by "hostname -f".
export MASTER_CANDIDATES="server1.domain.com,server2.domain.com" # Specify all or a subset of management hosts which will be defined as master candidates. The list of hostnames (FQDN as returned by "hostname -f" must be separated with comma (",")). If this parameter is defined, EGO_SHARED_DIR must also be defined. Only used by install-cluster.sh.

### HADOOP INTEGRATION
# Optional parameters to define if you are planning to use MapReduce framework and/or YARN integration on Symphony.
export JAVA_HOME= # Specify path to Java directory to use for MapReduce framework.
export HADOOP_HOME= # Specify the installation directory of the Hadoop distribution.
export HADOOP_VERSION= # Specify the version of the Hadoop distribution.
export DFS_GUI_HOSTNAME= # Specify the host name of the HDFS NameNode.
export DFS_GUI_PORT= # Specify the port number for the HDFS NameNode.
export HADOOP_YARN_HOME= # Specify the installation directory for full distribution YARN.

##### DIRECTORIES PARAMETERS
### BASE DIRECTORIES
export BASE_INSTALL_DIR=/opt/ibm/spectrum # If INSTALL_TYPE=shared, this directory must be on shared FS.
export EGO_SHARED_DIR=/opt/nfs/spectrum-ego-share # Optional, specify the EGO shared directory to use for High Availability (only useful if there are multiple management hosts). Must be on shared FS.

###### BELOW THIS POINT, YOU CAN KEEP DEFAULT CONFIGURATION UNLESS YOU WANT TO CUSTOMIZE IT #####

### INSTALL DIRECTORIES
export INSTALL_DIR=$BASE_INSTALL_DIR/install # Directory where Symphony will be installed. If INSTALL_TYPE=shared, it must be on shared FS.
export RPMDB_DIR=$BASE_INSTALL_DIR/rpmdb # Directory for Symphony RPMs database. If INSTALL_TYPE=shared, it must be on shared FS.

### SYMPHONY INSTALLER AND ENTITLEMENT
export SYMPHONY_BIN=`dirname "$(readlink -f "$0")"`/symphony/sym-7.3.0.0_`uname -m`.bin # Path to symphony 7.3.0 .bin installer.
export SYMPHONY_ENTITLEMENT=`dirname "$(readlink -f "$0")"`/symphony/sym_adv_entitlement.dat # Path to symphony 7.3.0 entitlement, either standard or advanced edition.

### IFIXES
export IFIX546970_EGOMGMT=`dirname "$(readlink -f "$0")"`/ifixes/egomgmt-3.8.0.0_noarch_build546970.tar.gz # Optional, if specified this ifix will be installed. Path to egomgmt .tar.gz for ifix 546970.

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
export PSSH_NBHOSTS_IN_PARALLEL=10 # Specify the number of management and compute hosts to install in parallel, only used by install-cluster.sh if pssh is installed and this parameter is greater than 1.
export PSSH_TIMEOUT=0 # Timeout in seconds for execution of the install on each host when using pssh. 0 means no timeout. Only used by install-cluster.sh if pssh is installed and parameter PSSH_NBHOSTS_IN_PARALLEL is greater than 1.
export INSTALL_FROM_RPMS=disabled # Specify enabled or disabled. If enabled, the script will extract rpms from Symphony bin installer and install from rpms with "--ignoresize" option (useful for Spectrum Scale shared install if you hit an error related to diskspace check).
export INSTALL_FROM_RPMS_TMP_DIR=$CACHE_DIR/rpms # Only required if INSTALL_FROM_RPMS=enabled. Specify directory where RPMs will be extracted.

### EGO DEFAULT SETTINGS
export EGO_ADMIN_USERNAME=Admin # Default Admin username after EGO installation.
export EGO_ADMIN_PASSWORD=Admin # Default Admin password after EGO installation.

### SSL
export SSL_TMP_DIR=$CACHE_DIR/ssl-files # Directory where SSL files will be written from the master host and need to be read by all other hosts. Need to be shared filesystem.
export SSL_CA_KEYSTORE_PASSWORD=Liberty # Default password of caKeyStore.jks keystore.
export SSL_TIER1_KEYSTORE_PASSWORD=Liberty # Default password of serverKeyStore.jks keystore.
