#!/bin/sh

##### GLOBAL PARAMETERS
export EGO_ADMIN_USERNAME=Admin # Username of the Admin account to use to create consumers and deploy applications
export EGO_ADMIN_PASSWORD=Admin # Password of the ADMIN_USER account

##### LAB ENVIRONMENT
### USER
export LAB_USER_BASE=user # Base user name which will have access (consumer admin role) to the Instance Groups and Anaconda instances created. The user name will be <LAB_USER_BASE>X, X being the first available number not already used
export LAB_PASSWORD=`date +%s%N | md5sum | head -c8` # Password for the user created
export LAB_CREATE_OS_USER=disabled # Specify enabled or disabled. If enabled, an OS user will be created

### ANACONDA DISTRIBUTION
export IG_ANACONDA_DISTRIBUTION_ID=Anaconda2019.10-Python3-Linux-${ARCH} # Anaconda distribution to be used by Instance Groups. If not specified, ANACONDA_DISTRIBUTION_ID_DEFAULT defined in parameters.inc will be used.

### INSTANCE GROUP WITH SPARK AND JUPYTER
export IG_SPARK243_CONDA_ENV_NAME=spark243 # Name of conda environment to create, which will be used by IG_SPARK243 Instance Group.
export IG_SPARK243_CONDA_ENV_PROFILE_TEMPLATE=`dirname "$(readlink -f "$0")"`/templates/CondaEnv-spark243.yaml # YAML profile for IG_SPARK243_CONDA_ENV_NAME conda environment.
export IG_SPARK243_BASENAME=spark243 # Name of Instance Group to create with Spark 2.4.3 and Jupyter notebook.
export IG_SPARK243_PROFILE_TEMPLATE=`dirname "$(readlink -f "$0")"`/templates/IG-spark243.json # JSON profile for IG_SPARK243 Instance Group.

### INSTANCE GROUPS FOR DLI
export IG_DLI_CONDA_ENV_NAME=dli # Name of conda environment to create, which will be used by IG_DLI and IG_DLIEDT Instance Groups.
export IG_DLI_CONDA_ENV_PROFILE_TEMPLATE=`dirname "$(readlink -f "$0")"`/templates/CondaEnv-dli.yaml # YAML profile for IG_DLI_CONDA_ENV_NAME conda environment.
export IG_DLI_BASENAME=dli # Name of Instance Group to create for Deep Learning Impact without EDT.
export IG_DLI_PROFILE_TEMPLATE=`dirname "$(readlink -f "$0")"`/templates/IG-dli.json # JSON profile for IG_DLI Instance Group.
export IG_DLIEDT_BASENAME=dliedt # Name of Instance Group to create for Deep Learning Impact with EDT.
export IG_DLIEDT_PROFILE_TEMPLATE=`dirname "$(readlink -f "$0")"`/templates/IG-dliedt.json # JSON profile for IG_DLIEDT Instance Group.

### INSTANCE GROUP SETTINGS
export IG_DEPLOY_TIMEOUT=600 # Timeout in seconds for Instance Group deployment.

##### ASSETS
### NOTEBOOKS
export NOTEBOOK_SOURCE_DIR="${BASE_SHARED_DIR}/WMLA-helpers/jupyter-notebooks" # Optional, if specified and exists, this directory content will be copied to the base user notebook directory when creating a notebook instance for user and invoking createSampleNotebooks() .  It should contain assets such as .ipynb and datasets.

### DEEP LEARNING
export DLI_DATASET_SOURCE_DIR="${BASE_SHARED_DIR}/CIFAR-10-images/train" # Optional and if path exist should contain a series of subfolders, one for each class within the image recognition dataset.  This dataset will be loaded through DLI and made available to the user.
export DLI_DATASET_BASENAME=cifar10 # Required if DLI_DATASET_SOURCE_DIR is defined, specify the base name of the dataset to create
export DLI_MODEL_SOURCE_DIR="${BASE_SHARED_DIR}/WMLA-helpers/dli-examples/model-pytorch-resnet" # Optional and if path exist should contain main.py and other elements for a pytorch model.  This directory is made available as a model template to the user.
export DLI_MODEL_BASENAME=resnet # Required if DLI_MODEL_SOURCE_DIR is defined, specify the base name of the model to create
