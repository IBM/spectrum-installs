#!/bin/sh

##### GLOBAL PARAMETERS
export EGO_ADMIN_USERNAME=Admin # Username of the Admin account to use to create consumers and deploy applications
export EGO_ADMIN_PASSWORD=Admin # Password of the ADMIN_USER account

##### LAB ENVIRONMENT
### USER SETTINGS
export LAB_USER_BASE=user # Base user name which will have access (consumer admin role) to the applications created. The user name will be <LAB_USER_BASE>X, X being the first available number not already used
export LAB_PASSWORD=`date +%s%N | md5sum | head -c8` # Password for the user created
export LAB_CREATE_OS_USER=enabled # Specify enabled or disabled. If enabled, an OS user will be created

export LAB_EXERCISES_TEMPLATES_DIR=/opt/nfs/spectrum-share/symphony/exercises # If specified and LAB_CREATE_OS_USER=enabled, this directory will be copied inside the OS user home directory

### DEMO SETTINGS
export DEMO_APP_PROFILE_TEMPLATE=`dirname "$(readlink -f "$0")"`/templates/app.xml # Application profile template to use to create applications.
export DEMO_VAR_APP_NAME=Demo7.3 # Name of the Value at Risk demo application installed by Symphony
export DEMO_VAR_CONSUMER_PATH=/SampleApplications/SOADemo # Consumer in which DEMO_VAR_APP_NAME is deployed
