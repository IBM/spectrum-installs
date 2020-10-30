#!/bin/sh

### GLOBAL PARAMETERS
export EGO_ADMIN_USERNAME=Admin # Username of the Admin account to use to create consumers and deploy applications
export EGO_ADMIN_PASSWORD=Admin # Password of the ADMIN_USER account

### LAB ENVIRONMENT
export LAB_USER_BASE=user # Base user name which will have access (consumer admin role) to the applications created. The user name will be <LAB_USER_BASE>X, X being the first available number not already used
export LAB_PASSWORD=`date +%s%N | md5sum | head -c8` # Password for the user created
