#!/bin/sh

source `dirname "$(readlink -f "$0")"`/conf/parameters.inc
source `dirname "$(readlink -f "$0")"`/functions/functions.inc
export LOG_FILE=$LOG_DIR/create-demo-env_`hostname -s`.log
[[ ! -d $LOG_DIR ]] && mkdir -p $LOG_DIR && chmod 777 $LOG_DIR

log "Starting create demo environment"

log "Wait for cluster to start and SD service to be started"
waitForClusterUp
waitForEgoServiceStarted SD

log "Creating user $DEMO_USER"
createUser $DEMO_USER $DEMO_PASSWORD

log "Creating consumers"
createConsumerStdCfg /MarketRisk
createConsumerStdCfg /MarketRisk/Equities
createConsumerStdCfg /MarketRisk/Equities/Equities-VaR
createConsumerStdCfg /MarketRisk/Equities/Equities-Intraday
createConsumerStdCfg /MarketRisk/FixedIncome
createConsumerStdCfg /MarketRisk/FixedIncome/FixedIncome-VaR
createConsumerStdCfg /MarketRisk/FixedIncome/FixedIncome-Intraday
createConsumerStdCfg /CreditRisk
createConsumerStdCfg /CreditRisk/Counterparty
createConsumerStdCfg /PreTrade
createConsumerStdCfg /PreTrade/Desk1
createConsumerStdCfg /PreTrade/Desk2

log "Creating sample applications"
createApplication $DEMO_APP_PROFILE_TEMPLATE Equities-VaR /MarketRisk/Equities/Equities-VaR $RG_COMPUTE_NAME $RG_MANAGEMENT_NAME
createApplication $DEMO_APP_PROFILE_TEMPLATE Equities-Intraday /MarketRisk/Equities/Equities-Intraday $RG_COMPUTE_NAME $RG_MANAGEMENT_NAME
createApplication $DEMO_APP_PROFILE_TEMPLATE FixedIncome-VaR /MarketRisk/FixedIncome/FixedIncome-VaR $RG_COMPUTE_NAME $RG_MANAGEMENT_NAME
createApplication $DEMO_APP_PROFILE_TEMPLATE FixedIncome-Intraday /MarketRisk/FixedIncome/FixedIncome-Intraday $RG_COMPUTE_NAME $RG_MANAGEMENT_NAME
createApplication $DEMO_APP_PROFILE_TEMPLATE CounterpartyCreditRisk /CreditRisk/Counterparty $RG_COMPUTE_NAME $RG_MANAGEMENT_NAME
createApplication $DEMO_APP_PROFILE_TEMPLATE PreTradeDesk1 /PreTrade/Desk1 $RG_COMPUTE_NAME $RG_MANAGEMENT_NAME
createApplication $DEMO_APP_PROFILE_TEMPLATE PreTradeDesk2 /PreTrade/Desk2 $RG_COMPUTE_NAME $RG_MANAGEMENT_NAME

log "Enabling VaR demo application"
enableApplication $DEMO_VAR_APP_NAME

log "Assigning Consumer Admin role to DEMO_USER for consumer $DEMO_VAR_CONSUMER_PATH in which demo application $DEMO_VAR_APP_NAME is deployed"
assignConsumerAdminRole $DEMO_USER $DEMO_VAR_CONSUMER_PATH

log "Demo environment created successfully!"
