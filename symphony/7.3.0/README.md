# Symphony 7.3.0 Setup scripts

## Table of Contents

[1. Description](#1-description)  
[2. Components installed](#2-components-installed)  
[3. Pre-requisites](#3-pre-requisites)  
[4. Usage](#4-usage)  
    [4.1. Download and prepare these scripts](#41-download-and-prepare-these-scripts)  
    [4.2. Download and prepare Symphony and ifix files](#42-download-and-prepare-symphony-and-ifix-files)  
    [4.3. Install the cluster](#43-install-the-cluster)  
    [4.4. Uninstall the cluster](#44-uninstall-the-cluster)  
[5. Description of files](#5-description-of-files)  
[6. Comments for SSL Certificates](#6-comments-for-ssl-certificates)  
[7. Info](#7-info)  
[7.1. Source repository](#71-source-repository)  
[7.2. Author](#72-author)  

## 1. Description
These scripts will install or uninstall IBM Spectrum Symphony 7.3.0 on a cluster of x86_64 or ppc64le servers, with RHEL Operating System, either local or shared install.  
It can also create a demo environment: user id, sample applications using symping and enable the Value-at-Risk demo application.  
Installation can be done using these bash scripts, or using the Ansible playbook.
Official documentation of IBM Spectrum Symphony is available [here](https://www.ibm.com/support/knowledgecenter/SSZUMP_7.3.0/sym_kc_welcome.html).

## 2. Components installed
* Symphony 7.3.0
* Ifix 546970 for WEBGUI (login issue due to cookie setting not recognized in Chrome and Safari)

## 3. Pre-requisites
* Servers need to be installed on a supported Linux OS version mentioned [here](https://www.ibm.com/support/knowledgecenter/SSZUMP_7.3.0/product_table/linux_support.html) and have the minimum hardware requirements mentioned [here](https://www.ibm.com/support/knowledgecenter/SSZUMP_7.3.0/install_grid_sym/sym_system_requirements.html).
* Servers need to be able to install few OS packages (using yum), either from local repository or through internet access, or these packages need to be already installed on all servers. The list of packages can be found in *prepare-host.sh* script.
* Python 2.7.x needs to be available on the servers. Path to python binary can be specified with *PYTHON_BIN* parameter in *parameters.inc* (by default "python").
* It is recommended to use these scripts from a shared filesystem accessible by all hosts. However if each node are installed individually without *install-cluster.sh*, scripts can be on local filesystem of each node and only the following parameters in *parameters.inc* need to be on a shared filesystem:
  * SSL_TMP_DIR (only used if SSL is enabled and *install-cluster.sh* or *update-ssl-host.sh* is executed)
  * SYNC_DIR (only used if *INSTALL_TYPE=local* and there are additional management hosts)
* If *install-cluster.sh* is used to install all hosts:
  * these scripts must be in a shared filesystem accessible by all hosts.
  * password-less SSH must be enabled for root from the host where this script is executed to all the hosts of the cluster.
  * optionally pssh package can be installed (from epel yum repo) in order to run installation on compute hosts in parallel.
* If the Ansible playbook is used to install the cluster:
  * Ansible must be installed on the host where the playbook will be executed.
  * The user used to execute the playbook must have password-less ssh access to all hosts of the cluster.
  * The user must have permissions to sudo as root as most tasks of the playbook will do privilege escalation. 

## 4. Usage

### 4.1. Download and prepare these scripts

#### 4.1.1. Download these scripts
Download and copy these scripts to a shared filesystem accessible by all hosts you are planning to install Symphony on.  
1. To download it:
```bash
git clone https://github.com/IBM/spectrum-installs.git
```

2. To copy Symphony 7.3.0 scripts:
```bash
cp -r spectrum-installs/symphony/7.3.0 <shared-filesystem>/symphony-7.3.0-install
```

#### 4.1.2. Edit parameters
Edit parameters in *conf/parameters.inc*. Mandatory parameters (at the top of the file) to change based on the target environment:
* INSTALL_TYPE
* CLUSTERADMIN
* CLUSTERNAME
* SSL
* MASTERHOST
* MASTER_CANDIDATES
* BASE_INSTALL_DIR
* EGO_SHARED_DIR

#### 4.1.3. Edit hosts list files
Add the list of servers to install (FQDN as returned by "hostname -f" command), 1 host per line, in the following 2 files:
* __conf/management-hosts.txt__ (or the file specified as *MANAGEMENTHOSTS_FILE* in *conf/parameters.inc*): List of management hosts (do not include the master).
* __conf/compute-hosts.txt__ (or the file specified as *COMPUTEHOSTS_FILE* in *conf/parameters.inc*): List of compute hosts (only used if the cluster is installed with *install-cluster.sh* or if *update-ssl-host.sh* is executed).

### 4.2. Download and prepare Symphony and ifix files

#### 4.2.1. Symphony
Either evaluation or entitled version can be used.  
Evaluation version of Symphony can be downloaded [here](https://epwt-www.mybluemix.net/software/support/trial/cst/welcomepage.wss?siteId=407&tabId=696&w=1&_ga=2.37481056.1963202311.1593446931-1746922340.1591820378&_gac=1.146273606.1592931153.Cj0KCQjw0Mb3BRCaARIsAPSNGpVvs4_x350LirZlBCCNA0vOZjO30Jacgk-lDhTE_4_ZvNNTu_glzkoaAvEoEALw_wcB&cm_mc_uid=21505324293715918203757&cm_mc_sid_50200000=17973571593490217046).  

#### 4.2.2. Ifix 546970
It can be downloaded [here](https://www.ibm.com/support/fixcentral/swg/downloadFixes?parent=IBM%20Spectrum%20Computing&product=ibm/Other+software/IBM+Spectrum+Symphony&release=7.3&function=fixId&fixids=sym-7.3-build546970).  

#### 4.2.3. Configure parameters.inc
By default these files are expected to be in the directory of the scripts, with this structure:
* symphony
  * sym-7.3.0.0_*ARCH*.bin
  * sym_adv_entitlement.dat
* ifixes
  * egomgmt-3.8.0.0_noarch_build546970.tar.gz

Path to these files can be changed in *conf/parameters.inc*.

### 4.3. Install the cluster

#### 4.3.1. Installing all hosts (recommended)
This is the recommended approach at it will install all hosts of the cluster automatically.
Execute the following script, as root, on any server having password-less ssh access to all hosts of the cluster:
```bash
./install-cluster.sh
```

#### 4.3.2. Installing using Ansible
The Ansible playbook *ansible-install-cluster.yaml* can be used to install the cluster.  
This playbook will execute the different scripts on each server.  
Steps:  
1. Execute *ansible-create-inventory.sh* to prepare the inventory file. Inventory will be created based on the configuration defined in the files in *conf* directory.  
2. Execute ansible playbook *ansible-install-cluster.yaml*:  
```bash
ansible-playbook ansible-install-cluster.yaml -i ansible-inventory.ini
```

#### 4.3.3. Installing each node individually

##### 4.3.3.1. Step by step
1. Execute *prepare-host.sh* as root on all servers.
2. Execute *install-host.sh* as root, on all servers if *INSTALL_TYPE=local* (starting with the master) or only on master if *INSTALL_TYPE=shared*.
3. If *SSL=enabled* and self-signed certificates will be used, execute *update-ssl-host.sh* as root on all hosts starting with the master host.
4. Execute *postinstall-host.sh* as root on all servers.
5. If there are multiple management nodes, master host need to be restarted to take them into account:
```bash
su -l $CLUSTERADMIN -c "source $INSTALL_DIR/profile.platform && egosh ego restart -f"
```
6. To create demo environment, execute *create-demo-environment.sh* on master. It will create a user id, sample applications using symping and enable the Value-at-Risk demo application.
7. If there are multiple management nodes, the master candidates list need to be configured either from Symphony GUI or with this CLI:
```bash
su -l $CLUSTERADMIN -c "source $INSTALL_DIR/profile.platform && egoconfig masterlist $MASTER_CANDIDATES -f && egosh ego restart -f"
```

##### 4.3.3.2. Short version
1. Local install / SSL enabled - Execute on master host:
```bash
./prepare-host.sh && ./install-host.sh && ./update-ssl-host.sh && ./postinstall-host.sh
```
2. Local install / SSL enabled - Execute on additional management hosts and compute hosts (make sure *update-ssl-host.sh* is executed on master host before executing it on other hosts):
```bash
./prepare-host.sh && ./install-host.sh && ./update-ssl-host.sh && ./postinstall-host.sh
```
3. Shared install / SSL enabled - Execute on master host:
```bash
./prepare-host.sh && ./install-host.sh && ./update-ssl-host.sh && ./postinstall-host.sh
```
4. Shared install / SSL enabled - Execute on additional management hosts and compute hosts (make sure *update-ssl-host.sh* is executed on master host before executing it on other hosts):
```bash
../prepare-host.sh && ./postinstall-host.sh
```

### 4.4. Uninstall the cluster
__WARNING__: Make sure that no change was made in *parameters.inc*, *management-hosts.txt* and *compute-hosts.txt*, as the uninstall scripts will use these configuration files to know which hosts to uninstall, and which directories to delete.  
__WARNING__: These scripts will __NOT__ ask any confirmation before stopping and uninstalling the cluster.  

#### 4.4.1. Uninstalling all hosts
Execute the following script, as root, on any server having password-less ssh access to all hosts of the cluster:
```bash
./forceuninstall-cluster.sh
```

#### 4.4.2. Uninstalling using Ansible
The Ansible playbook *ansible-forceuninstall-cluster.yaml* can be used to uninstall the cluster.  
This playbook will execute the different scripts on each server.  
Steps:  
1. If needed, execute *ansible-create-inventory.sh* to prepare the inventory file. Inventory will be created based on the configuration defined in the files in *conf* directory.  
2. Execute ansible playbook *ansible-forceuninstall-cluster.yaml*:  
```bash
ansible-playbook ansible-forceuninstall-cluster.yaml -i ansible-inventory.ini
```

#### 4.4.3. Uninstalling each node individually
1. Execute the following script, as root, on each host of the cluster, starting with the master host:
```bash
./forceuninstall-host.sh
```
2. Delete the shared directories:
* EGO_SHARED_DIR

## 5. Description of files
* __README.md__: Description of the scripts and how to use.
* __prepare-host.sh__: Script to prepare current host before installation.
* __install-host.sh__: Installation script (Install all the components on current host).
* __install-cluster.sh__: Cluster installation script (Install all the components on all hosts and create Instance Groups).
* __postinstall-host.sh__: Post-installation script (Define rc init script for Symphony and EGO sudoers on current host).
* __update-ssl-host.sh__: Script to update SSL self-signed certificates and keystores to include all hostnames.
* __create-demo-environment.sh__: Script to create a demo environment (user id and sample applications).
* __forceuninstall-host.sh__: Uninstall Symphony on current host (stop EGO services, stop EGO on the current host and delete *BASE_INSTALL_DIR*).
* __forceuninstall-cluster.sh__: Uninstall Symphony on all hosts (stop EGO services, stop EGO on all hosts, delete *BASE_INSTALL_DIR* on all hosts, delete *EGO_SHARED_DIR*).
* __ansible-create-inventory.sh__: Create the Ansible inventory file to be used with *ansible-install-cluster.yaml* and *ansible-forceuninstall-cluster.yaml* playbooks.
* __ansible-install-cluster.yaml__: Ansible playbook to install the cluster.
* __ansible-forceuninstall-cluster.yaml__: Ansible playbook to uninstall the cluster.
* __test-scripts.sh__: Script to test that these install scripts work properly. It should only be used by developers of these scripts.
* __conf/__:
    * __parameters.inc__: Parameters for the installation.
    * __management-hosts.txt__: File containing list of management hosts of the cluster.
    * __compute-hosts.txt__: File containing list of compute hosts of the cluster.
* __functions/__:
    * __functions.inc__: Include all functions files.
    * __functions-common.inc__: Common functions for scripts.
    * __functions-cluster-management.inc__: Functions to manage cluster.
    * __functions-soam-management.inc__: Functions to manage SOAM.
    * __functions-ssl.inc__: Functions to update self-signed certificates and keystores.
* __templates/__:
    * __app.xml__: Application profile template for the demo environment.

## 6. Comments for SSL Certificates
* *update-ssl-host.sh* script will generate self-signed certificates with "IBM Spectrum Computing Root CA" certificate authority. In order to avoid security alerts in the browser when accessing the web interface, follow the step 3 of [this documentation](https://www.ibm.com/support/knowledgecenter/SSZUMP_7.3.0/help/admin/locating_pmc.html).
* To import external certificates, do not run *update-ssl-host.sh* script and follow the documentation to import external certificates available [here](https://www.ibm.com/support/knowledgecenter/SSZUMP_7.3.0/security/security_https.html).

## 7. Info
### 7.1. Source repository
The repository of these scripts is located at [https://github.com/IBM/spectrum-installs](https://github.com/IBM/spectrum-installs).  

### 7.2. Author
Anthony Frery, afrery@us.ibm.com  
Feel free to reach out if you have any question!
