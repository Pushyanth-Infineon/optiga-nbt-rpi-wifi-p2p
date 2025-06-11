# SPDX-FileCopyrightText: Copyright (c) 2025 Infineon Technologies AG
# SPDX-License-Identifier: MIT

#!/bin/bash

# /etc/dhcpcd.conf have static ip for p2p-dev-wlan0
ip_address=""
connection_name=wifi-p2p-p2p-dev-wlan0

wpa_cli -i p2p-dev-wlan0 p2p_stop_find
wpa_cli -i p2p-dev-wlan0 set config_methods virtual_push_button
wpa_cli -i p2p-dev-wlan0 p2p_find

# List all the devices
peers=$(wpa_cli -i p2p-dev-wlan0 p2p_peers)

while true; do
    peers=$(wpa_cli -i p2p-dev-wlan0 p2p_peers)
    len_peers=$(echo $peers | wc -w)
    for i in $peers; do 
        echo -n "$i ";
        wpa_cli -i p2p-dev-wlan0 p2p_peer $i | grep device_name=;
    done

    read -t 2 -rp "Press enter to start providing MAC address..."
    if [[ $? == 0 ]]; then
        break
    fi
    echo

    tput cuu $(($len_peers+1))
    tput ed
done

read -p "Enter the MAC address of the device: " mac_address

if [[ $(sudo nmcli -g name con | grep $connection_name) != "" ]]; then
    echo "Connection: $connection_name already exists. Removing and replacing"
    sudo nmcli connection delete $connection_name
fi

# Use nmcli to add p2p-network and connect to it
sudo nmcli connection add con-name wifi-p2p-p2p-dev-wlan0 connection.type wifi-p2p\
        ifname p2p-dev-wlan0 wifi-p2p.wps-method pbc wifi-p2p.peer $mac_address autoconnect no

if [[ $? == 0 ]]; then
    sudo nmcli con up wifi-p2p-p2p-dev-wlan0
else
    echo "Failed to add connection.."
    exit 1
fi

if [[ $? != 0 ]]; then
    echo "Failed to establish P2P connection"
    exit
fi

# Get IP address of p2p interface
ip_address=$(ip addr show $(ls -d /sys/class/net/p2p-wlan* | xargs basename ) | grep -o "inet [0-9]*\.[0-9]*\.[0-9]*\.[0-9]*" | grep -o "[0-9]*\.[0-9]*\.[0-9]*\.[0-9]*")
python3 $(dirname $0)/socket_server_activity.py $ip_address
