#!/bin/sh

##### GLOBAL PARAMETERS
### CLUSTER INSTALL
export INSTALL_TYPE=local # Specify shared or local. Type of installation for WMLA binaries.
export DEPLOYMENT_TYPE=shared # Only required if INSTALL_TYPE=local. Specify shared or local. Type of deployment for Instance Groups, Anaconda instances and Application Instances.
export CLUSTERADMIN=egoadmin # OS user who will be cluster admin.
export CLUSTERNAME=WMLA # Display name of the cluster.
export SSL=enabled # Specify enabled or disabled. If enabled, all web interfaces and REST services will use SSL.
export MASTERHOST=server1.domain.com # FQDN of master host of the cluster, as returned by "hostname -f".
export MASTER_CANDIDATES= # Specify all or a subset of management hosts which will be defined as master candidates. The list of hostnames (FQDN as returned by "hostname -f" must be separated with comma (",") and complete list enclosed with double quote). If this parameter is defined, EGO_SHARED_DIR must also be defined. Only used by install-cluster.sh.

### ALL HOSTS INSTALL
export CLUSTERINSTALL_CREATE_LAB_ENVIRONMENT=enabled # Specify enabled or disabled. If enabled, script install-cluster.sh will create lab environment based on parameters defined in lab-environment.inc.
export CLUSTERINSTALL_UPDATE_SSL=enabled # Specify enabled or disabled. If enabled, script install-cluster.sh will execute update-ssl-host.sh.

##### DIRECTORIES PARAMETERS
### BASE DIRECTORIES
export BASE_INSTALL_DIR=/opt/ibm/spectrum # If INSTALL_TYPE=shared, this directory must be on shared FS.
export BASE_SHARED_DIR=/opt/nfs/spectrum-share # Must be on shared FS.
export EGO_SHARED_DIR= # Optional, specify the EGO shared directory to use for High Availability (only useful if there are multiple management hosts). Must be on shared FS.

###### BELOW THIS POINT, YOU CAN KEEP DEFAULT CONFIGURATION UNLESS YOU WANT TO CUSTOMIZE IT #####

### FIND DEPLOYMENT DIRECTORY BASED ON INSTALL_TYPE AND DEPLOYMENT_TYPE
if [ "$INSTALL_TYPE" == "shared" -o "$DEPLOYMENT_TYPE" == "shared" ]
then
  DEPLOYMENT_DIR=$BASE_SHARED_DIR
else
  DEPLOYMENT_DIR=$BASE_INSTALL_DIR
fi

### DEPLOYMENT DIRECTORIES
export IG_DIR=$DEPLOYMENT_DIR/instance-groups # Specify base directory where Instance Groups will be deployed. If INSTALL_TYPE=shared or DEPLOYMENT_TYPE=shared, it must be on shared FS.
export ANACONDA_DIR=$DEPLOYMENT_DIR/anaconda # Specify base directory where Anaconda instances will be deployed. If INSTALL_TYPE=shared or DEPLOYMENT_TYPE=shared, it must be on shared FS.

### INSTALL DIRECTORIES
export INSTALL_DIR=$BASE_INSTALL_DIR/install # Directory where WMLA will be installed. If INSTALL_TYPE=shared, it must be on shared FS.
export RPMDB_DIR=$BASE_INSTALL_DIR/rpmdb # Directory for WMLA RPMs database. If INSTALL_TYPE=shared, it must be on shared FS.
if [ "$INSTALL_TYPE" == "local" -a "$DEPLOYMENT_TYPE" == "shared" ]
then
  export ELASTIC_HARVEST_DIR=$BASE_SHARED_DIR/elastic_logs # Only required if INSTALL_TYPE=local and DEPLOYMENT_TYPE=shared. Specify the directory which Filebeat will use. It must be on shared FS.
fi

### SUB-DIRECTORIES UNDER BASE DIRECTORIES
export SPARKHA_DIR=$BASE_SHARED_DIR/spark-ha # Directory that Spark of Instance Groups will use for High Availability. It must be on shared FS.
export SPARKHISTORY_DIR=$BASE_SHARED_DIR/spark-history # Directory that Spark History service of Instance Groups will use. It must be on shared FS.
export SPARKSHUFFLE_DIR=$BASE_SHARED_DIR/spark-shuffle # Directory that Spark Shuffle service of Instance Groups will use. It must be on shared FS.
export NOTEBOOKS_DIR=$BASE_SHARED_DIR/notebooks # Directory that Jupyter notebooks of Instance Groups will use for data. It must be on shared FS.

export DLI_SHARED_FS=$BASE_SHARED_DIR/dli_shared_fs # Directory that DLI will use for configurations, binaries and data. It must be on shared FS.
export DLI_DATA_FS=$BASE_SHARED_DIR/dli_data_fs # Directory that DLI will use for datasets storage. It must be on shared FS.
export DLI_RESULT_FS=$BASE_SHARED_DIR/dli_result_fs # Directory that DLI will use for user specific job data storage. It must be on shared FS.

##### ARCHITECTURE (x86_64 or ppc64le)
export ARCH=`uname -m` # Identify current architecture of the server.

##### ANACONDA PARAMETERS
### COMMON
export ANACONDA_DISTRIBUTION_ID_DEFAULT=Anaconda4.8.3-Python3-Linux-${ARCH} # Default Anaconda distribution installed by Conductor, which will be used if ANACONDA_DISTRIBUTION_NAME_TO_ADD is not specified.
export ANACONDA_DISTRIBUTIONS_ID_TO_DELETE="" # Optional, list (space separated) of the default Anaconda distribution installed by Conductor, which will be deleted.
export ANACONDA_DISTRIBUTION_NAME_TO_ADD= # Optional, specify a new Anaconda distribution to install on Conductor, which will be use for the different conda environments. The name must be a filename available on ANACONDA_DISTRIB_REPO_BASE_URL minus the ".sh" extension. You can download the file in advance and put it in $CACHE_DIR/$ANACONDA_DISTRIBUTION_NAME_TO_ADD.sh, or the script will download it.
export CONDA_OPENCE_ARCHIVE=`dirname "$(readlink -f "$0")"`/wmladl/wmladl-cuda11.0-py37_${ARCH}.tgz # Specify the path to the WMLA-DL (OpenCE) archive
export CONDA_OPENCE_DIR=$BASE_INSTALL_DIR/wmladl # Specify the path to the directory where WMLA-DL will be untar

### ANACONDA PARAMETERS FOR DLINSIGHTS
export DLI_ANACONDA_DISTRIBUTION_ID= # Anaconda distribution to be used by dlinsights. If not specified, ANACONDA_DISTRIBUTION_ID_DEFAULT will be used.
export DLI_ANACONDA_INSTANCE_NAME=WMLA-dlinsights # Anaconda instance to create, to be used by dlinsights.
export DLI_ANACONDA_INSTANCE_DEPLOY_HOME=$ANACONDA_DIR/$DLI_ANACONDA_INSTANCE_NAME # Directory where Anaconda instance for dlinsights will be deployed.
export DLI_CONDA_HOME=$DLI_ANACONDA_INSTANCE_DEPLOY_HOME/anaconda # Directory of anaconda folder within the Anaconda instance deployment directory, which will be used by dlinsights.
export DLI_CONDA_DLINSIGHTS_NAME=dlinsights # Conda environment name used by dlinsights EGO service. Cannot be changed as DLI is expecting this specific name.
export DLI_CONDA_DLINSIGHTS_PROFILE_TEMPLATE=`dirname "$(readlink -f "$0")"`/templates/CondaEnv-dlinsights.yaml # YAML profile for dlinsights conda environment.

##### RESOURCE GROUP PARAMETERS
export RG_GPU_NAME=ComputeHostsGPU # Optional, name of GPU Resource Group to create. If not specified, no GPU Resource Group will be created.
export RG_CPU_NB_SLOTS_PER_CORE=1 # Optional, if specified and different from 1 (1 being the default value), the Resource Group RG_CPU_NAME (ComputeHosts by default) will be updated with this number of slots per core.

##### PATH TO BINARIES

### CONDUCTOR INSTALLER AND ENTITLEMENT
export CONDUCTOR_BIN=`dirname "$(readlink -f "$0")"`/conductor/conductor2.5.0.0_${ARCH}.bin # Path to conductor 2.5.0 .bin installer.
export CONDUCTOR_ENTITLEMENT=`dirname "$(readlink -f "$0")"`/conductor/conductor_entitlement.dat # Path to conductor 2.5.0 entitlement.

### DLI INSTALLER AND ENTITLEMENT
export DLI_BIN=`dirname "$(readlink -f "$0")"`/dli/dli-1.2.5.0_${ARCH}.bin # Path to DLI 1.2.5 .bin installer.
export DLI_ENTITLEMENT=`dirname "$(readlink -f "$0")"`/dli/dli_entitlement.dat # Path to DLI 1.2.5 entitlement.

##### SCRIPTS PARAMETERS
### WORKING DIRECTORIES
export CACHE_DIR=`dirname "$(readlink -f "$0")"`/cache # Base directory that these scripts will use to create temporary files.
export LOG_DIR=`dirname "$(readlink -f "$0")"`/logs # Directory where all log files will be written.
export SYNC_DIR=$CACHE_DIR/sync # Only required if INSTALL_TYPE=local and there are additional management hosts. Directory where install-host.sh script will write lock file to avoid conflict for the parallel execution of the script on different hosts.
export SCRIPTS_TMP_DIR=$CACHE_DIR/generated-scripts # Only required if you use install-cluster.sh. Directory where install-cluster.sh will generate scripts to install each type of host, and run them remotely.

### COMMON
export MANAGEMENTHOSTS_FILE=`dirname "$(readlink -f "$0")"`/conf/management-hosts.txt # Path of file containing list of additional management hosts (FQDN as returned by "hostname -f"), 1 host per line.
export COMPUTEHOSTS_FILE=`dirname "$(readlink -f "$0")"`/conf/compute-hosts.txt # Path of file containing list of compute hosts (FQDN as returned by "hostname -f"), 1 host per line. Only used by install-cluster.sh and update-ssl-host.sh.
export PYTHON_BIN=python # Path to Python binary. Must be python 2.7.x.
export JQ_BIN=jq # Path to jq binary.
export STATUS_CHECK_WAITTIME=5 # Duration in seconds to wait before 2 check of status.
export EGO_SHUTDOWN_WAITTIME=15 # Duration in seconds to wait after stopping or restarting EGO.
export ANACONDA_DISTRIB_REPO_BASE_URL="https://repo.anaconda.com/miniconda/" # Base URLs to download Anaconda distributions.
export CONDA_ENV_CREATION_NB_ATTEMPT=3 # Number of time the script will try to create each Conda environment (useful when you use WMLCE conda channel which sometimes return HTTP error 503)
export CONDA_ENV_CREATION_WAITTIME=60 # Duration in seconds to wait before each attempt (except the first) of creating each Conda environment.
export UPDATE_RP_SCRIPT=`dirname "$(readlink -f "$0")"`/functions/update-resource-plan.py # Python script to update share ratio of Instance Groups consumers in the resource plan.

### INSTALLATION
export SSH_PORT=22 # Optional, specify the port for SSH connection. Only used by install-cluster.sh and forceuninstall-cluster.sh.
export PSSH_NBHOSTS_IN_PARALLEL=10 # Specify the number of management and compute hosts to install in parallel, only used by install-cluster.sh if pssh is installed and this parameter is greater than 1.
export PSSH_TIMEOUT=0 # Timeout in seconds for execution of the install on each host when using pssh. 0 means no timeout. Only used by install-cluster.sh if pssh is installed and parameter PSSH_NBHOSTS_IN_PARALLEL is greater than 1.
export INSTALL_FROM_RPMS=disabled # Specify enabled or disabled. If enabled, the script will extract rpms from Conductor and DLI bin installers and install from rpms with "--ignoresize" option (useful for Spectrum Scale shared install if you hit an error related to diskspace check).
export INSTALL_FROM_RPMS_TMP_DIR=$CACHE_DIR/rpms # Only required if INSTALL_FROM_RPMS=enabled. Specify directory where RPMs will be extracted.
export CLUSTERINSTALL_WAITTIME_BEFORE_CREATE_LAB_ENVIRONMENT=120 # Duration in seconds to wait for compute hosts to join the cluster before creating Lab Environment (only used by install-cluster.sh).

### ANSIBLE
export ANSIBLE_INVENTORY_FILE=`dirname "$(readlink -f "$0")"`/ansible-inventory.ini # Path of Ansible inventory file that will be generated by ansible-create-inventory.sh and can be used by ansible-install-cluster.yaml and ansible-forceuninstall-cluster.yaml playbooks

### ANACONDA LOCAL CHANNEL
export ANACONDA_LOCAL_CHANNEL=disabled # Specify enabled or disabled. If enabled, script prepare-local-conda-channel.sh will create the conda local channel, and installation of conda environments will be done using the local channel from ANACONDA_LOCAL_CHANNEL_DIR if folder exists, or from ANACONDA_LOCAL_CHANNEL_ARCHIVE if file exists.
export ANACONDA_LOCAL_CONDA_ENV_CREATION_TYPE=profile # Specify profile or twosteps. If profile is specified, conda environment will be created with the profile. If twosteps if specified, empty conda environment will be created first, then packages from the profile will be installed.
export ANACONDA_LOCAL_CONDA_ENV_CREATION_NB_ATTEMPT=3 # Number of time the script will try to create each Conda environment (useful when you use WMLCE conda channel which sometimes return HTTP error 503)
export ANACONDA_LOCAL_CONDA_ENV_CREATION_WAITTIME=60 # Duration in seconds to wait before each attempt (except the first) of creating each Conda environment.
export ANACONDA_LOCAL_DISTRIBUTION_NAME=Miniconda3-py37_4.8.3-Linux-${ARCH} # Only required if you use prepare-local-conda-channel.sh. Specify the new Anaconda distribution to download and install to prepare conda packages to create the local conda channel.
export ANACONDA_LOCAL_INSTALL_DIR=$CACHE_DIR/anaconda-local-install # Only required if you use prepare-local-conda-channel.sh. Directory where Anaconda will be installed to prepare conda packages to create the local conda channel.
export ANACONDA_LOCAL_CHANNEL_DIR=$CACHE_DIR/anaconda-local-channel # Only required if you use prepare-local-conda-channel.sh. Specify the directory of the local conda channel.
export ANACONDA_LOCAL_CHANNEL_ARCHIVE=$CACHE_DIR/anaconda-local-channel.tgz # Only required if you use prepare-local-conda-channel.sh. Specify the name of the archive (.tgz) of the local conda channel.
export ANACONDA_LOCAL_CHANNEL_STRICT=disabled # Only required if you use prepare-local-conda-channel.sh. Specify enabled or disabled. If enabled, creation of conda environments will only use the local channel. It needs to be enabled if you are in an airgap environment.

### ANACONDA AIRGAP INSTALL
export ANACONDA_AIRGAP_INSTALL=disabled # Specify enabled or disabled. If enabled, script prepare-airgap-install.sh will create the needed conda environments and package them in ANACONDA_AIRGAP_INSTALL_DLINSIGHTS_ARCHIVE and ANACONDA_AIRGAP_INSTALL_IG_ARCHIVE_BASENAME-X.tgz. During installation, dlinsights service and Instance Groups will use the conda environments which will be created from ANACONDA_AIRGAP_INSTALL_DLINSIGHTS_ARCHIVE and ANACONDA_AIRGAP_INSTALL_IG_ARCHIVE.
export ANACONDA_AIRGAP_NB_LAB_ENVS=1 # Specify the number of lab environments for which conda environments will be prepared.
export ANACONDA_AIRGAP_CONDA_ENV_CREATION_NB_ATTEMPT=3 # Number of time the script will try to create each Conda environment (useful when you use WMLCE conda channel which sometimes return HTTP error 503)
export ANACONDA_AIRGAP_CONDA_ENV_CREATION_WAITTIME=60 # Duration in seconds to wait before each attempt (except the first) of creating each Conda environment.
export ANACONDA_AIRGAP_DISTRIBUTION_NAME=Miniconda3-py37_4.8.3-Linux-${ARCH} # Only required if you use prepare-airgap-install.sh. Specify the new Anaconda distribution to download and install to prepare conda packages to create the local conda channel.
export ANACONDA_AIRGAP_INSTALL_DLINSIGHTS_ARCHIVE=$CACHE_DIR/anaconda-airgap-install-dlinsights.tgz # Only required if you use prepare-airgap-install.sh. Specify the name of the archive (.tgz) with the dlinsights conda environment.
export ANACONDA_AIRGAP_INSTALL_IG_ARCHIVE_BASENAME=$CACHE_DIR/anaconda-airgap-install-ig # Only required if you use prepare-airgap-install.sh. Specify the name of the basename archive with the conda environments for Instance Groups. For each lab environment to prepare (number defined in ANACONDA_AIRGAP_NB_LAB_ENVS) an archive <ANACONDA_AIRGAP_INSTALL_IG_ARCHIVE_BASENAME>-<LAB_ENV_NB>.tgz will be created.

### EGO DEFAULT SETTINGS
export EGO_ADMIN_USERNAME=Admin # Default Admin username after EGO installation.
export EGO_ADMIN_PASSWORD=Admin # Default Admin password after EGO installation.
export RG_MGMT_NAME=ManagementHosts # Default Resource Group with management hosts after EGO installation.
export RG_CPU_NAME=ComputeHosts # Default Resource Group with compute hosts after EGO installation.

### SSL
export SSL_TMP_DIR=$CACHE_DIR/ssl-files # Directory where SSL files will be written from the master host and need to be read by all other hosts. Need to be shared filesystem.
export SSL_CA_KEYSTORE_PASSWORD=Liberty # Default password of caKeyStore.jks keystore.
export SSL_TIER1_KEYSTORE_PASSWORD=Liberty # Default password of serverKeyStore.jks keystore.
export SSL_TIER23_KEYSTORE_PASSWORD=SparkPassword # Default password of tier2and3ServerKeyStore.jks keystore.
export SSL_TIER23_KEYSTORE_TIER2ALIAS_PASSWORD=password_tier2 # Password to use for tier 2 alias in tier2and3ServerKeyStore.jks keystore.
export SSL_TIER23_KEYSTORE_TIER3ALIAS_PASSWORD=password_tier3 # Password to use for tier 3 alias in tier2and3ServerKeyStore.jks keystore.
export SSL_TIER3_KEYSTORE_PASSWORD=pkcs12-password # Default password of tier3KeyStore.p12 keystore.
export SSL_TIER3_KEYSTORE_KEY_PASSWORD=pkcs12-key-passwd # Password to use for tier3opensslprivate.key key file.
