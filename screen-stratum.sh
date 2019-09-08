#!/bin/bash

 LOG_DIR=/var/log
 WEB_DIR=//var/www/pool.kryptofranc.net/html/web/
 STRATUM_DIR=/var/stratum
 USR_BIN=/usr/bin

 gnome-terminal -e "/var/stratum/run.sh sha" --working-directory "/var/stratum"
 gnome-terminal -e "/var/stratum/run.sh shafranc"  --working-directory "/var/stratum"
 #screen -dmS sha256franc $STRATUM_DIR/run.sh sha-franc
 #screen -dmS kyf-qt "/home/pool/Kryptofranc/binaries/unix/kyf-qt -deprecatedrpc=accounts -printtoconsole -feedbackfee=0.0002"
 gnome-terminal -e  "/home/pool/Kryptofranc/binaries/unix/kyf-qt -deprecatedrpc=accounts -printtoconsole -fallbackfee=0.0002"
 screen -dmS main bash /var/www/pool.kryptofranc.net/html/web//main.sh
 screen -dmS loop2 bash /var/www/pool.kryptofranc.net/html/web//loop2.sh
 screen -dmS blocks bash /var/www/pool.kryptofranc.net/html/web//blocks.sh
 #screen -dmS debug tail -f $LOG_DIR/debug.log


 #screen -dmS keccak $STRATUM_DIR/run.sh keccak
 #screen -dmS neoscrypt $STRATUM_DIR/run.sh neo
 #screen -dmS nist5 $STRATUM_DIR/run.sh nist5
 #screen -dmS quark $STRATUM_DIR/run.sh quark
 #screen -dmS scrypt $STRATUM_DIR/run.sh scrypt
 #screen -dmS skein $STRATUM_DIR/run.sh skein
 #screen -dmS x11 $STRATUM_DIR/run.sh x11
 #screen -dmS xevan $STRATUM_DIR/run.sh xevan
 
