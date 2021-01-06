#!/bin/sh

##### Tier 1

updateSslTier1OnMaster() {
  log "Backup keystore, gui/conf/server_gui.xml and kernel/conf/server_rest.xml"
  cp $EGO_TOP/wlp/usr/shared/resources/security/serverKeyStore.jks $EGO_TOP/wlp/usr/shared/resources/security/serverKeyStore_BACKUP-`date "+%Y-%m-%d-%H-%M-%S"`.jks 2>&1 | tee -a $LOG_FILE
  cp $EGO_CONFDIR/../../gui/conf/server_gui.xml $EGO_CONFDIR/../../gui/conf/server_gui_BACKUP-`date "+%Y-%m-%d-%H-%M-%S"`.xml 2>&1 | tee -a $LOG_FILE
  cp $EGO_CONFDIR/server_rest.xml $EGO_CONFDIR/server_rest_BACKUP-`date "+%Y-%m-%d-%H-%M-%S"`.xml 2>&1 | tee -a $LOG_FILE

  log "Remove tier1 certificate from serverKeyStore.jks"
  $KEYTOOL_BIN -delete -alias srvalias -keystore $EGO_TOP/wlp/usr/shared/resources/security/serverKeyStore.jks -storepass $SSL_TIER1_KEYSTORE_PASSWORD 2>&1 | tee -a $LOG_FILE

  log "Add tier1 certificate in serverKeyStore.jks"
  $KEYTOOL_BIN -genkeypair -noprompt -alias srvalias -dname "CN=*$SSL_DOMAIN,O=IBM,C=CA" -keystore $EGO_TOP/wlp/usr/shared/resources/security/serverKeyStore.jks -storepass "$SSL_TIER1_KEYSTORE_PASSWORD" -keypass "$SSL_TIER1_KEYSTORE_PASSWORD" -keyalg rsa -validity 825 -keysize  2048 -sigalg SHA256withRSA -ext "san=$SSL_MANAGEMENT_HOSTNAMES_LIST" 2>&1 | tee -a $LOG_FILE

  log "Create the certificate-signing request file for tier1"
  $KEYTOOL_BIN -certreq -alias srvalias -file srvcertreq.csr -storepass "$SSL_TIER1_KEYSTORE_PASSWORD" -keystore $EGO_TOP/wlp/usr/shared/resources/security/serverKeyStore.jks -ext "san=$SSL_MANAGEMENT_HOSTNAMES_LIST" 2>&1 | tee -a $LOG_FILE

  log "Sign tier1 certificate"
  $KEYTOOL_BIN -gencert -infile srvcertreq.csr -outfile servercertcasigned.pem -alias caalias -keystore $EGO_TOP/wlp/usr/shared/resources/security/caKeyStore.jks -storepass "$SSL_CA_KEYSTORE_PASSWORD" -validity 825 -keysize 2048 -sigalg SHA256withRSA -keyalg rsa -ext "san=$SSL_MANAGEMENT_HOSTNAMES_LIST" 2>&1 | tee -a $LOG_FILE

  log "Import signed tier1 certificate in serverKeyStore.jks"
  $KEYTOOL_BIN  -importcert -noprompt -alias srvalias -file servercertcasigned.pem -storepass "$SSL_TIER1_KEYSTORE_PASSWORD" -keypass "$SSL_TIER1_KEYSTORE_PASSWORD" -keystore $EGO_TOP/wlp/usr/shared/resources/security/serverKeyStore.jks 2>&1 | tee -a $LOG_FILE

  log "Encrypt passwords of tier1 certificate"
  TIER1_PASS=`$SECURITYUTILITY_BIN encode --encoding=aes "$SSL_TIER1_KEYSTORE_PASSWORD"`
  sed -i 's#<keyStore id="defaultKeyStore" password=".*" type="JKS"#<keyStore id="defaultKeyStore" password="'$TIER1_PASS'" type="JKS"#' $EGO_CONFDIR/../../gui/conf/server_gui.xml 2>&1 | tee -a $LOG_FILE
  sed -i 's#<keyStore id="defaultKeyStore" password=".*" type="JKS"#<keyStore id="defaultKeyStore" password="'$TIER1_PASS'" type="JKS"#' $EGO_CONFDIR/server_rest.xml 2>&1 | tee -a $LOG_FILE

  log "Deleting temporary files"
  rm -f srvcertreq.csr 2>&1 | tee -a $LOG_FILE
  rm -f servercertcasigned.pem 2>&1 | tee -a $LOG_FILE

  log "Copy SSL files to $SSL_TMP_DIR"
  cp -f -p $EGO_TOP/wlp/usr/shared/resources/security/serverKeyStore.jks $SSL_TMP_DIR 2>&1 | tee -a $LOG_FILE
}

updateSslTier1OnNonMaster() {
  log "Copy SSL files from $SSL_TMP_DIR"
  cp -f -p $SSL_TMP_DIR/serverKeyStore.jks $EGO_TOP/wlp/usr/shared/resources/security/ 2>&1 | tee -a $LOG_FILE
}
