#!/bin/sh

##### Tier 1

updateSslTier1OnMaster() {
  log "Backup keystore, gui/conf/server_gui.xml, ascd/conf/server_ascd.xml, kernel/conf/server_rest.xml and dli/conf/dlpd/server_dlrest.xml"
  cp $EGO_TOP/wlp/usr/shared/resources/security/serverKeyStore.jks $EGO_TOP/wlp/usr/shared/resources/security/serverKeyStore_BACKUP-`date "+%Y-%m-%d-%H-%M-%S"`.jks 2>&1 | tee -a $LOG_FILE
  cp $EGO_CONFDIR/../../gui/conf/server_gui.xml $EGO_CONFDIR/../../gui/conf/server_gui_BACKUP-`date "+%Y-%m-%d-%H-%M-%S"`.xml 2>&1 | tee -a $LOG_FILE
  cp $EGO_CONFDIR/../../ascd/conf/server_ascd.xml $EGO_CONFDIR/../../ascd/conf/server_ascd_BACKUP-`date "+%Y-%m-%d-%H-%M-%S"`.xml 2>&1 | tee -a $LOG_FILE
  cp $EGO_CONFDIR/server_rest.xml $EGO_CONFDIR/server_rest_BACKUP-`date "+%Y-%m-%d-%H-%M-%S"`.xml 2>&1 | tee -a $LOG_FILE
  cp $EGO_CONFDIR/../../dli/conf/dlpd/server_dlrest.xml $EGO_CONFDIR/../../dli/conf/dlpd/server_dlrest_BACKUP-`date "+%Y-%m-%d-%H-%M-%S"`.xml 2>&1 | tee -a $LOG_FILE

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
  sed -i 's#<keyStore id="defaultKeyStore" password=".*" type="JKS"#<keyStore id="defaultKeyStore" password="'$TIER1_PASS'" type="JKS"#' $EGO_CONFDIR/../../ascd/conf/server_ascd.xml 2>&1 | tee -a $LOG_FILE
  sed -i 's#<keyStore id="defaultKeyStore" password=".*" type="JKS"#<keyStore id="defaultKeyStore" password="'$TIER1_PASS'" type="JKS"#' $EGO_CONFDIR/server_rest.xml 2>&1 | tee -a $LOG_FILE
  sed -i 's#<keyStore id="defaultKeyStore" password=".*" type="JKS"#<keyStore id="defaultKeyStore" password="'$TIER1_PASS'" type="JKS"#' $EGO_CONFDIR/../../dli/conf/dlpd/server_dlrest.xml 2>&1 | tee -a $LOG_FILE

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

##### Tier 2 and 3

updateSslTier23OnMaster() {
  log "Backup tier2and3ServerKeyStore.jks and ascd/conf/ssl.conf"
  cp $EGO_TOP/security/tier2and3ServerKeyStore.jks $EGO_TOP/security/tier2and3ServerKeyStore_BACKUP-`date "+%Y-%m-%d-%H-%M-%S"`.jks 2>&1 | tee -a $LOG_FILE
  cp $EGO_CONFDIR/../../ascd/conf/ssl.conf $EGO_CONFDIR/../../ascd/conf/ssl_BACKUP-`date "+%Y-%m-%d-%H-%M-%S"`.conf 2>&1 | tee -a $LOG_FILE

  log "Remove tier2 and tier3 certificates from tier2and3ServerKeyStore.jks"
  $KEYTOOL_BIN -delete -alias tier2alias -keystore $EGO_TOP/security/tier2and3ServerKeyStore.jks -storepass "$SSL_TIER23_KEYSTORE_PASSWORD" 2>&1 | tee -a $LOG_FILE
  $KEYTOOL_BIN -delete -alias tier3alias -keystore $EGO_TOP/security/tier2and3ServerKeyStore.jks -storepass "$SSL_TIER23_KEYSTORE_PASSWORD" 2>&1 | tee -a $LOG_FILE

  log "Add tier2 and tier3 certificates in tier2and3ServerKeyStore.jks"
  $KEYTOOL_BIN -genkeypair -noprompt -alias tier2alias -dname "CN=*$SSL_DOMAIN,O=IBM,C=CA" -keystore $EGO_TOP/security/tier2and3ServerKeyStore.jks -storepass "$SSL_TIER23_KEYSTORE_PASSWORD" -keypass "$SSL_TIER23_KEYSTORE_TIER2ALIAS_PASSWORD" -keyalg rsa -validity 825 -keysize  2048 -sigalg SHA256withRSA -ext "san=$SSL_ALL_HOSTNAMES_LIST" 2>&1 | tee -a $LOG_FILE
  $KEYTOOL_BIN -genkeypair -noprompt -alias tier3alias -dname "CN=*$SSL_DOMAIN,O=IBM,C=CA" -keystore $EGO_TOP/security/tier2and3ServerKeyStore.jks -storepass "$SSL_TIER23_KEYSTORE_PASSWORD" -keypass "$SSL_TIER23_KEYSTORE_TIER3ALIAS_PASSWORD" -keyalg rsa -validity 825 -keysize  2048 -sigalg SHA256withRSA -ext "san=$SSL_ALL_HOSTNAMES_LIST" 2>&1 | tee -a $LOG_FILE

  log "Create the certificate-signing request file for tier2 and tier3"
  $KEYTOOL_BIN -certreq -alias tier2alias -file tier2alias.csr -keystore $EGO_TOP/security/tier2and3ServerKeyStore.jks -storepass "$SSL_TIER23_KEYSTORE_PASSWORD" -keypass "$SSL_TIER23_KEYSTORE_TIER2ALIAS_PASSWORD" 2>&1 | tee -a $LOG_FILE
  $KEYTOOL_BIN -certreq -alias tier3alias -file tier3alias.csr -keystore $EGO_TOP/security/tier2and3ServerKeyStore.jks -storepass "$SSL_TIER23_KEYSTORE_PASSWORD" -keypass "$SSL_TIER23_KEYSTORE_TIER3ALIAS_PASSWORD" 2>&1 | tee -a $LOG_FILE

  log "Sign tier2 and tier3 certificates"
  $KEYTOOL_BIN -gencert -infile tier2alias.csr -outfile tier2aliascertcasigned.pem -alias caalias -keystore $EGO_TOP/wlp/usr/shared/resources/security/caKeyStore.jks -storepass "$SSL_CA_KEYSTORE_PASSWORD" -validity 825 -keysize 2048 -sigalg SHA256withRSA -keyalg rsa -ext "san=$SSL_ALL_HOSTNAMES_LIST" 2>&1 | tee -a $LOG_FILE
  $KEYTOOL_BIN -gencert -infile tier3alias.csr -outfile tier3aliascertcasigned.pem -alias caalias -keystore $EGO_TOP/wlp/usr/shared/resources/security/caKeyStore.jks -storepass "$SSL_CA_KEYSTORE_PASSWORD" -validity 825 -keysize 2048 -sigalg SHA256withRSA -keyalg rsa -ext "san=$SSL_ALL_HOSTNAMES_LIST" 2>&1 | tee -a $LOG_FILE

  log "Import signed tier2 and tier3 certificates in tier2and3ServerKeyStore.jks"
  $KEYTOOL_BIN  -import -noprompt -alias tier2alias -file tier2aliascertcasigned.pem -storepass "$SSL_TIER23_KEYSTORE_PASSWORD" -keypass "$SSL_TIER23_KEYSTORE_TIER2ALIAS_PASSWORD" -keystore $EGO_TOP/security/tier2and3ServerKeyStore.jks 2>&1 | tee -a $LOG_FILE
  $KEYTOOL_BIN  -import -noprompt -alias tier3alias -file tier3aliascertcasigned.pem -storepass "$SSL_TIER23_KEYSTORE_PASSWORD" -keypass "$SSL_TIER23_KEYSTORE_TIER3ALIAS_PASSWORD" -keystore $EGO_TOP/security/tier2and3ServerKeyStore.jks 2>&1 | tee -a $LOG_FILE

  log "Encrypt passwords of tier2 and tier3 certificates"
  TIER2_PASS=`$SECURITYUTILITY_BIN encode --encoding=aes "$SSL_TIER23_KEYSTORE_TIER2ALIAS_PASSWORD"`
  sed -i "s#TIER2_KEY_PASSWD=.*#TIER2_KEY_PASSWD=$TIER2_PASS#" $EGO_CONFDIR/../../ascd/conf/ssl.conf 2>&1 | tee -a $LOG_FILE
  TIER3_PASS=`$SECURITYUTILITY_BIN encode --encoding=aes "$SSL_TIER23_KEYSTORE_TIER3ALIAS_PASSWORD"`
  sed -i "s#TIER3_KEY_PASSWD=.*#TIER3_KEY_PASSWD=$TIER3_PASS#" $EGO_CONFDIR/../../ascd/conf/ssl.conf 2>&1 | tee -a $LOG_FILE

  log "Change ownership of tier2and3ServerKeyStore.jks to $CLUSTERADMIN"
  chown $CLUSTERADMIN:$CLUSTERADMIN $EGO_TOP/security/tier2and3ServerKeyStore.jks 2>&1 | tee -a $LOG_FILE

  log "Convert the pkcs12 keystore from the jks keystore for tier3 certificate"
  mv $EGO_TOP/security/tier3KeyStore.p12 $EGO_TOP/security/tier3KeyStore_BACKUP-`date "+%Y-%m-%d-%H-%M-%S"`.p12 2>&1 | tee -a $LOG_FILE
  $KEYTOOL_BIN -importkeystore -srckeystore $EGO_TOP/security/tier2and3ServerKeyStore.jks -srcalias tier3alias -srcstoretype jks -srcstorepass "$SSL_TIER23_KEYSTORE_PASSWORD" -srckeypass "$SSL_TIER23_KEYSTORE_TIER3ALIAS_PASSWORD" -destkeystore $EGO_TOP/security/tier3KeyStore.p12 -deststoretype pkcs12 -deststorepass "$SSL_TIER3_KEYSTORE_PASSWORD" -destkeypass "$SSL_TIER3_KEYSTORE_KEY_PASSWORD" -noprompt 2>&1 | tee -a $LOG_FILE
  chown $CLUSTERADMIN:$CLUSTERADMIN $EGO_TOP/security/tier3KeyStore.p12 2>&1 | tee -a $LOG_FILE

  log "Create the private key file for tier3"
  mv $EGO_TOP/security/tier3opensslprivate.key $EGO_TOP/security/tier3opensslprivate_BACKUP-`date "+%Y-%m-%d-%H-%M-%S"`.key 2>&1 | tee -a $LOG_FILE
  openssl pkcs12 -in $EGO_TOP/security/tier3KeyStore.p12 -passin pass:$SSL_TIER3_KEYSTORE_PASSWORD -nocerts -out $EGO_TOP/security/tier3opensslprivate.key -passout pass:$SSL_TIER3_KEYSTORE_KEY_PASSWORD 2>&1 | tee -a $LOG_FILE
  chown $CLUSTERADMIN:$CLUSTERADMIN $EGO_TOP/security/tier3opensslprivate.key 2>&1 | tee -a $LOG_FILE

  log "Create public certificate for tier3"
  mv $EGO_TOP/security/tier3opensslpublic.pem $EGO_TOP/security/tier3opensslpublic_BACKUP-`date "+%Y-%m-%d-%H-%M-%S"`.pem 2>&1 | tee -a $LOG_FILE
  openssl pkcs12 -in $EGO_TOP/security/tier3KeyStore.p12 -passin pass:$SSL_TIER3_KEYSTORE_PASSWORD -nokeys -out $EGO_TOP/security/tier3opensslpublic.pem 2>&1 | tee -a $LOG_FILE
  chown $CLUSTERADMIN:$CLUSTERADMIN $EGO_TOP/security/tier3opensslpublic.pem 2>&1 | tee -a $LOG_FILE

  log "Encrypt password of tier3 private key"
  TIER3_OPENSSL_PASS=`$SECURITYUTILITY_BIN encode --encoding=aes "$SSL_TIER3_KEYSTORE_KEY_PASSWORD"`
  sed -i "s#TIER3_OPENSSL_PEM_PASSWD=.*#TIER3_OPENSSL_PEM_PASSWD=$TIER3_OPENSSL_PASS#" $EGO_CONFDIR/../../ascd/conf/ssl.conf 2>&1 | tee -a $LOG_FILE

  log "Deleting temporary files"
  rm -f tier2aliascertcasigned.pem 2>&1 | tee -a $LOG_FILE
  rm -f tier3aliascertcasigned.pem 2>&1 | tee -a $LOG_FILE
  rm -f tier2alias.csr 2>&1 | tee -a $LOG_FILE
  rm -f tier3alias.csr 2>&1 | tee -a $LOG_FILE

  log "Copy SSL files to $SSL_TMP_DIR"
  cp -f -p $EGO_TOP/security/tier2and3ServerKeyStore.jks $SSL_TMP_DIR 2>&1 | tee -a $LOG_FILE
  cp -f -p $EGO_TOP/security/tier3KeyStore.p12 $SSL_TMP_DIR 2>&1 | tee -a $LOG_FILE
  cp -f -p $EGO_TOP/security/tier3opensslprivate.key $SSL_TMP_DIR 2>&1 | tee -a $LOG_FILE
  cp -f -p $EGO_TOP/security/tier3opensslpublic.pem $SSL_TMP_DIR 2>&1 | tee -a $LOG_FILE
}

updateSslTier23OnNonMaster() {
  log "Copy SSL files from $SSL_TMP_DIR"
  cp -f -p $SSL_TMP_DIR/tier2and3ServerKeyStore.jks $EGO_TOP/security/ 2>&1 | tee -a $LOG_FILE
  cp -f -p $SSL_TMP_DIR/tier3KeyStore.p12 $EGO_TOP/security/ 2>&1 | tee -a $LOG_FILE
  cp -f -p $SSL_TMP_DIR/tier3opensslprivate.key $EGO_TOP/security/ 2>&1 | tee -a $LOG_FILE
  cp -f -p $SSL_TMP_DIR/tier3opensslpublic.pem $EGO_TOP/security/ 2>&1 | tee -a $LOG_FILE
}
