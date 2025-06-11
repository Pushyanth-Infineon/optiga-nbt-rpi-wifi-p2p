# SPDX-FileCopyrightText: Copyright (c) 2025 Infineon Technologies AG
# SPDX-License-Identifier: MIT

#!/bin/bash

source $(dirname $0)/config_files.sh

if [[ $(apt -qq list nmap  2>/dev/null | grep "\[installed\]$" ) == "" ]]; then
    echo "Installing nmap"
    sudo apt-get update
    sudo apt-get install nmap
fi

if [[ -f $wpa_supplicant_file ]]; then
    echo "$wpa_supplicant_file exists. Overwriting it.."
fi

sudo bash -c "cat > $wpa_supplicant_file <<EOF
ctrl_interface=DIR=/var/run/wpa_supplicant GROUP=netdev
update_config=1
country=DE
device_name=DIRECT-RasPi1

# If you need to modify the group owner intent, 0-15, the higher
# number indicates preference to become the GO. You can also set
# this on p2p_connect commands.
p2p_go_intent=15

# In order to support 802.11n for the p2p Group Owner
p2p_go_ht40=1

# Device type
#   1-0050F204-1 (Computer / PC)
#   1-0050F204-2 (Computer / Server)
#   5-0050F204-1 (Storage / NAS)
#   6-0050F204-1 (Network Infrastructure / AP)
device_type=6-0050F204-1
driver_param=p2p_device=6
EOF"


sudo chmod 644 $wpa_supplicant_file

# Add wpa_supplicant p2p config file flag to the wpa_supplicant.serivce
sudo sed -i 's/^ExecStart=\/sbin\/wpa_supplicant -u -s -O "DIR=\/run\/wpa_supplicant GROUP=netdev".*/ExecStart=\/sbin\/wpa_supplicant -u -s -O "DIR=\/run\/wpa_supplicant GROUP=netdev" -m \/etc\/wpa_supplicant\/wpa_supplicant-p2p-dev-wlan0.conf/' $wpa_supplicant_service

echo "Restarting wpa_supplicant service"
sudo systemctl restart wpa_supplicant.service

echo "Reloading systemd manager configuration"
sudo systemctl daemon-reload

if [ -S /var/run/wpa_supplicant/p2p-dev-wlan0 ]; then
    echo "p2p-dev-wlan0 does not exist in: /var/run/wpa_supplicant/"
    exit 1
fi
