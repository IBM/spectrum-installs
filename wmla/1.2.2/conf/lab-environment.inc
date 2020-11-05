#!/bin/sh

### GLOBAL PARAMETERS
export EGO_ADMIN_USERNAME=Admin # Username of the Admin account to use to create consumers and deploy applications
export EGO_ADMIN_PASSWORD=Admin # Password of the ADMIN_USER account

### LAB ENVIRONMENT
export LAB_USER_BASE=user # Base user name which will have access (consumer admin role) to the applications created. The user name will be <LAB_USER_BASE>X, X being the first available number not already used
export LAB_PASSWORD=`date +%s%N | md5sum | head -c8` # Password for the user created

##### ASSETS
### NOTEBOOKS
export NOTEBOOK_SOURCE_DIR="${BASE_SHARED_DIR}/WMLA-helpers/jupyter-notebooks" # Optional, if specified and exists, this directory content will be copied to the base user notebook directory when creating a notebook instance for user and invoking createSampleNotebooks() .  It should contain assets such as .ipynb and datasets.

### DEEP LEARNING
export DLI_DATASET_SOURCE_DIR="${BASE_SHARED_DIR}/CIFAR-10-images/train" # Optional and if path exist should contain a series of subfolders, one for each class within the image recognition dataset.  This dataset will be loaded through DLI and made available to the user.
export DLI_MODEL_SOURCE_DIR="${BASE_SHARED_DIR}/WMLA-helpers/dli-examples/model-pytorch-resnet" # Optional and if path exist should contain main.py and other elements for a pytorch model.  This directory is made available as a model template to the user.
