# WMLA 1.2.2 Setup scripts

## Table of Contents

[1. Description](#1-description)  
[2. Components installed](#2-components-installed)  
[3. Pre-requisites](#3-pre-requisites)  
[4. Usage](#4-usage)  
    [4.1. Download and prepare these scripts](#41-download-and-prepare-these-scripts)  
    [4.2. Download and prepare WMLA and ifix files](#42-download-and-prepare-wmla-and-ifix-files)  
    [4.3. Additional steps to use local conda channel](#43-additional-steps-to-use-local-conda-channel)  
    [4.4. Additional steps for airgap environment](#44-additional-steps-for-airgap-environment)  
    [4.5. Install the cluster](#45-install-the-cluster)  
    [4.6. Uninstall the cluster](#46-uninstall-the-cluster)  
[5. Description of files](#5-description-of-files)  
[6. Comments for SSL Certificates](#6-comments-for-ssl-certificates)  
[7. Info](#7-info)  
[7.1. Source repository](#71-source-repository)  
[7.2. Author](#72-author)  

## 1. Description
These scripts will install or uninstall Watson Machine Learning Accelerator 1.2.2 on a cluster of x86_64 or ppc64le servers, either local or shared install.  
It can also create 3 Instance Groups, 1 with Spark 2.4.3 and Jupyter notebook and 2 for Deep Learning Impact component (1 for EDT and 1 for non EDT).  
Installation can be done in an airgap environment (environment without internet access) if needed.  
Official documentation of WMLA is available [here](https://www.ibm.com/support/knowledgecenter/SSFHA8_1.2.2/wmla_kc_welcome.html).

## 2. Components installed
* Conductor 2.4.1
* Deep Learning Impact 1.2.4
* Ifix 546962 for WEBGUI (login issue due to cookie setting not recognized in Chrome and Safari)

## 3. Pre-requisites
* Servers need to be installed on a supported OS version (RHEL 7.7 on x86, RHEL 7.6 on Power) and have the minimum hardware requirements mentioned [here](https://www.ibm.com/support/knowledgecenter/SSFHA8_1.2.2/wmla_hwandsw_reqs.html).
* If there are GPUs in server, nvidia driver needs to be installed.
* Servers need to be able to install few OS packages (using yum), either from local repository or through internet access, or these packages need to be already installed on all servers. The list of packages can be found in *prepare-host.sh* script.
* Python 2.7.x needs to be available on the servers. Path to python binary can be specified with *PYTHON_BIN* parameter in *parameters.inc* (by default "python").
* It is recommended to use these scripts from a shared filesystem accessible by all hosts. However if each node are installed individually without *install-cluster.sh*, scripts can be on local filesystem of each node and only the following parameters in *parameters.inc* need to be on a shared filesystem:
  * SSL_TMP_DIR (only used if SSL is enabled and *update-ssl-host.sh* is executed)
  * SYNC_DIR (only used if *INSTALL_TYPE=local* and there are additional management hosts)
  * ANACONDA_LOCAL_CHANNEL_DIR and ANACONDA_LOCAL_CHANNEL_ARCHIVE (only used if *ANACONDA_LOCAL_CHANNEL=enabled*)
  * ANACONDA_AIRGAP_INSTALL_DLINSIGHTS_ARCHIVE and ANACONDA_AIRGAP_INSTALL_IG_ARCHIVE (only used if *ANACONDA_AIRGAP_INSTALL=enabled*)
* If *install-cluster.sh* is used to install all hosts:
  * these scripts must be in a shared filesystem accessible by all hosts.
  * password-less SSH must be enabled for root from the host where this script is executed to all hosts of the cluster (except for the host where it's executed).
  * optionally pssh package can be installed (from epel yum repo) in order to run installation on management and compute hosts in parallel.

## 4. Usage

### 4.1. Download and prepare these scripts

#### 4.1.1. Download these scripts
Download and copy these scripts to a shared filesystem accessible by all hosts you are planning to install WMLA on.  
1. To download it:
```bash
git clone https://github.com/IBM/spectrum-installs.git
```

2. To copy WMLA 1.2.2 scripts:
```bash
cp -r spectrum-installs/wmla/1.2.2 <shared-filesystem>/wmla-1.2.2-install
```

#### 4.1.2. Edit parameters
Edit parameters in *conf/parameters.inc*. Mandatory parameters (at the top of the file) to change based on the target environment:
* INSTALL_TYPE
* DEPLOYMENT_TYPE
* CLUSTERADMIN
* CLUSTERNAME
* SSL
* MASTERHOST
* MASTER_CANDIDATES
* CLUSTERINSTALL_CREATE_USER_ENVIRONMENT
* CLUSTERINSTALL_UPDATE_SSL
* BASE_INSTALL_DIR
* BASE_SHARED_DIR
* EGO_SHARED_DIR
* IG_DIR
* ANACONDA_DIR
* INSTALL_DIR
* RPMDB_DIR  

#### 4.1.3. Edit hosts list files
Add the list of servers to install (FQDN as returned by "hostname -f" command), 1 host per line, in the following 2 files:
* __conf/management-hosts.txt__ (or the file specified as *MANAGEMENTHOSTS_FILE* in *conf/parameters.inc*): List of management hosts (do not include the master).
* __conf/compute-hosts.txt__ (or the file specified as *COMPUTEHOSTS_FILE* in *conf/parameters.inc*): List of compute hosts (only used if the cluster is installed with *install-cluster.sh* or if *update-ssl-host.sh* is executed).

### 4.2. Download and prepare WMLA and ifix files

#### 4.2.1. WMLA
Either evaluation or entitled version can be used.  
Evaluation version of WMLA can be downloaded [here](https://epwt-www.mybluemix.net/software/support/trial/cst/programwebsite.wss?siteId=303&tabId=569&w=1&_ga=2.268747569.1108201166.1591021143-1361767747.1588637100&cm_mc_uid=31929065010115886370980&cm_mc_sid_50200000=53944041591203718578).  
Once WMLA file downloaded:
* Extract the content of the archive:
```bash
tar xzf wmla-1.2.2*.tar.gz
```
* Extract install files:
```bash
export IBM_WMLA_LICENSE_ACCEPT=yes
sh ibm-wmla-1.2.2*.bin
```

#### 4.2.2. Ifix 546962
It can be downloaded [here](https://www.ibm.com/support/fixcentral/swg/selectFixes?product=ibm/Other+software/IBM+Spectrum+Conductor+with+Spark&release=All&platform=All&function=fixId&fixids=sc-2.4.1-build546962&includeSupersedes=0).  

#### 4.2.3. Configure parameters.inc
By default these files are expected to be in the directory of the scripts, with this structure:
* conductor
  * conductor2.4.1.0_*ARCH*.bin
  * conductor_entitlement.dat
* dli
  * dli-1.2.4.0_*ARCH*.bin
  * dli_entitlement.dat
* ifixes
  * egomgmt-3.8.0.1_noarch_build546962.tar.gz  

Path to these files can be changed in *conf/parameters.inc*.  

If the evaluation version is used, these 4 parameters need to be updated with the correct filenames:  
* CONDUCTOR_BIN
* CONDUCTOR_ENTITLEMENT
* DLI_BIN
* DLI_ENTITLEMENT

If ifix 546962 is not used, parameter *IFIX546962_EGOMGMT* need to be commented in *conf/parameters.inc*.  

### 4.3. Additional steps to use local conda channel

In order to accelerate installation, a local conda channel can be prepared, and conda environments will be created using this channel.  
For some conda environments, even if all packages are available in the local conda channel, anaconda will still connect to Internet. Therefore for complete airgap installation, follow steps in section 4.4.  

Follow the steps below to prepare the local conda channel.  

#### 4.3.1. Configure parameters.inc
Edit following parameters in *conf/parameters.inc* in order to enable conda local channel:
* *ANACONDA_LOCAL_CHANNEL*: Enable local channel.
* *ANACONDA_LOCAL_DISTRIBUTION_NAME*: Anaconda distribution to use to create the local conda channel.
* *ANACONDA_DISTRIBUTION_NAME_TO_ADD*: New Anaconda distribution to download, which will be added to WMLA.

#### 4.3.2. Download and prepare required files
In order to download Anaconda distribution and create the local conda channel, execute the following script on a server with Internet access, which has the same architecture (x86_64 or ppc64le) than the target environment, and where the user defined in *CLUSTERADMIN* of *conf/parameters.inc* exists:
```bash
./prepare-local-conda-channel.sh
```

#### 4.3.3. Copy files
Copy the files prepared by *prepare-local-conda-channel.sh* in the scripts folder which will be used to install the cluster:
* Anaconda distribution file need to be under *CACHE_DIR*
* Anaconda local channel can be copied as the folder (under *ANACONDA_LOCAL_CHANNEL_DIR*) or as the archive (in *ANACONDA_LOCAL_CHANNEL_ARCHIVE*) under *CACHE_DIR*

### 4.4. Additional steps for airgap environment

The installation of WMLA requires in most case files and packages which need to be downloaded from Internet or internal repositories:
* OS packages.  
* Anaconda distribution, to use instead of the one provided out-of-the-box by WMLA.  
* Conda packages for dlinsights service.  
* Conda packages for conda environments used by the Instance Groups if created.  

Follow the steps below to download and prepare these files.  

If both *ANACONDA_LOCAL_CHANNEL* and *ANACONDA_AIRGAP_INSTALL* are enabled, airgap install method will be used to create the conda environments, and not the local conda channel.  

#### 4.4.1. Install OS packages

Check if the OS packages required by WMLA are already installed on the servers, and if not install them either from local repository or by downloading and installing them manually.  
This list of packages can be found in the script *prepare-host.sh*, line starting with "yum install".  

#### 4.4.2. Configure parameters.inc
Edit following parameters in *conf/parameters.inc* in order to enable airgap installation:
* *ANACONDA_AIRGAP_INSTALL*: Enable airgap installation.
* *ANACONDA_AIRGAP_DISTRIBUTION_NAME*: Anaconda distribution to use to create the conda environments.
* *ANACONDA_DISTRIBUTION_NAME_TO_ADD*: New Anaconda distribution to download, which will be added to WMLA.

#### 4.4.3. Download and prepare required files
In order to download Anaconda distribution and create the conda environments, execute the following script on a server with Internet access, which has the same architecture (x86_64 or ppc64le) than the target environment, and where the user defined in *CLUSTERADMIN* of *conf/parameters.inc* exists:
```bash
./prepare-airgap-install.sh
```

#### 4.4.4. Copy files
Copy the files prepared by *prepare-airgap-install.sh* in the scripts folder which will be used to install the cluster:
* Anaconda distribution file need to be under *CACHE_DIR*
* Archives containing conda environments need to be copied under *CACHE_DIR*

### 4.5. Install the cluster

#### 4.5.1. Installing all hosts (recommended)
This is the recommended approach as it will install all hosts of the cluster automatically.
Execute the following script, as root, on any server having password-less ssh access to all hosts of the cluster:
```bash
./install-cluster.sh
```

#### 4.5.2. Installing each node individually

##### 4.5.2.1. Step by step
1. Execute *prepare-host.sh* as root on all servers.
2. Execute *install-host.sh* as root, on all servers if *INSTALL_TYPE=local* (starting with the master) or only on master if *INSTALL_TYPE=shared*.
3. If *SSL=enabled* and self-signed certificates will be used, execute *update-ssl-host.sh* as root on all hosts starting with the master host.
4. Execute *postinstall-host.sh* as root on all servers.
5. To create User environment, execute *create-user-environment.sh* on master. It will create a user id, Anaconda instance, conda environments and 3 Instance Groups (1 with Spark 2.4.3 and Jupyter notebook, 1 with Spark 2.3.3 for DLI with EDT and 1 with Spark 2.3.3 for DLI without EDT). At least 1 compute host with GPUs need to be available in the cluster in order to have GPU resource group configured.
6. If there are multiple management nodes, the master candidates list need to be configured either from WMLA GUI or with this CLI:
```bash
su -l $CLUSTERADMIN -c "source $INSTALL_DIR/profile.platform && egoconfig masterlist $MASTER_CANDIDATES -f"
```

##### 4.5.2.2. Short version
1. Local install / SSL enabled - Execute on master host:
```bash
./prepare-host.sh && ./install-host.sh && ./update-ssl-host.sh && ./postinstall-host.sh
```
2. Local install / SSL enabled - Execute on additional management hosts and compute hosts (make sure *update-ssl-host.sh* is executed on master host before executing it on other hosts):
```bash
./prepare-host.sh && ./install-host.sh && ./update-ssl-host.sh && ./postinstall-host.sh
```
2. Shared install / SSL enabled - Execute on master host:
```bash
./prepare-host.sh && ./install-host.sh && ./update-ssl-host.sh && ./postinstall-host.sh
```
3. Shared install / SSL enabled - Execute on additional management hosts and compute hosts:
```bash
./prepare-host.sh && ./postinstall-host.sh
```

### 4.6. Uninstall the cluster
__WARNING__: Make sure that no change was made in *parameters.inc*, *management-hosts.txt* and *compute-hosts.txt*, as the uninstall scripts will use these configuration files to know which hosts to uninstall, and which directories to delete.  
__WARNING__: These scripts will __NOT__ ask any confirmation before stopping and uninstalling the cluster.  

#### 4.6.1. Uninstalling all hosts
Execute the following script, as root, on any server having password-less ssh access to all hosts of the cluster:
```bash
./forceuninstall-cluster.sh
```

#### 4.6.2. Uninstalling each node individually
1. Execute the following script, as root, on each host of the cluster, starting with the master host:
```bash
./forceuninstall-host.sh
```
2. Delete the shared directories:
* BASE_SHARED_DIR
* EGO_SHARED_DIR

## 5. Description of files
* __README.md__: Description of the scripts and how to use.
* __prepare-host.sh__: Script to prepare current host before installation.
* __install-host.sh__: Installation script (Install all the components on current host).
* __install-cluster.sh__: Cluster installation script (Install all the components on all hosts and create Instance Groups).
* __postinstall-host.sh__: Post-installation script (Define rc init script for WMLA and EGO sudoers on current host).
* __update-ssl-host.sh__: Script to update SSL self-signed certificates and keystores to include all hostnames.
* __create-user-environment.sh__: Create 3 Instance Groups (1 with Spark 2.4.3 and Jupyter notebook, 1 for DLI with EDT and 1 for DLI without EDT).
* __prepare-local-conda-channel.sh__: Script to download Anaconda distribution and create a local conda channel.
* __prepare-airgap-install.sh__: Script to download Anaconda distribution and create conda environments.
* __forceuninstall-host.sh__: Uninstall WMLA on current host (stop EGO services, stop EGO on the current host and delete *BASE_INSTALL_DIR*).
* __forceuninstall-cluster.sh__: Uninstall WMLA on all hosts (stop EGO services, stop EGO on all hosts, delete *BASE_INSTALL_DIR* on all hosts, delete *BASE_SHARED_DIR* and *EGO_SHARED_DIR*).
* __conf/__:
    * __parameters.inc__: Parameters for the installation.
    * __management-hosts.txt__: File containing list of management hosts of the cluster.
    * __compute-hosts.txt__: File containing list of compute hosts of the cluster.
* __functions/__:
    * __functions.inc__: Include all functions files.
    * __functions-common.inc__: Common functions for scripts.
    * __functions-cluster-management.inc__: Functions to manage cluster.
    * __functions-ssl.inc__: Functions to update self-signed certificates and keystores.
    * __functions-anaconda.inc__: Functions to manage Anaconda distributions and instances.
    * __functions-instance-groups.inc__: Functions to manage Instance Groups.
    * __update-resource-plan.py__: Script to update resource plan for Instance Groups.
* __templates/__:
    * __CondaEnv-dlinsights.yaml__: Conda environment profile for dlinsights EGO service.
    * __CondaEnv-dli.yaml__: Conda environment profile for dliedt Instance Group.
    * __CondaEnv-spark243.yaml__: Conda environment profile for spark243 Instance Group.
    * __IG-dli.json__: Instance Group profile for DLI.
    * __IG-dliedt.json__: Instance Group profile for DLI with Elastic Distributed Training.
    * __IG-spark243.json__: Instance Group profile for spark243.

## 6. Comments for SSL Certificates
* *update-ssl-host.sh* script will generate self-signed certificates with "IBM Spectrum Computing Root CA" certificate authority. In order to avoid security alerts in the browser when accessing the web interface, follow the step 3 of [this documentation](https://www.ibm.com/support/knowledgecenter/SSZU2E_2.4.1/get_started/locating_pmc.html).
* To import external certificates, do not run *update-ssl-host.sh* script and follow the documentation to import external certificates available [here](https://www.ibm.com/support/knowledgecenter/SSZU2E_2.4.1/manage_cluster/security_https.html).

## 7. Info
### 7.1. Source repository
The repository of these scripts is located at [https://github.com/IBM/spectrum-installs](https://github.com/IBM/spectrum-installs).  

### 7.2. Author
Anthony Frery, afrery@us.ibm.com  
Feel free to reach out if you have any question!
