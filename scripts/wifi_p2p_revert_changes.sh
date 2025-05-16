#!/bin/bash

source $(dirname $0)/config_files.sh

# Remove wpa_supplicant p2p config file from wpa_supplicant.service
sudo sed -i 's/^ExecStart=\/sbin\/wpa_supplicant -u -s -O "DIR=\/run\/wpa_supplicant GROUP=netdev".*/ExecStart=\/sbin\/wpa_supplicant -u -s -O "DIR=\/run\/wpa_supplicant GROUP=netdev"/' $wpa_supplicant_service

sudo rm $wpa_supplicant_file

echo "Restarting wpa_supplicant service"
sudo systemctl restart wpa_supplicant.service

# TODO: Check if its needed 
# sudo systemctl restart NetworkManager.service

echo "Reloading systemd manager configuration"
sudo systemctl daemon-reload

