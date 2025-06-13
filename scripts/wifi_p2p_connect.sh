# SPDX-FileCopyrightText: Copyright (c) 2025 Infineon Technologies AG
# SPDX-License-Identifier: MIT

#!/bin/bash

# /etc/dhcpcd.conf have static ip for p2p-dev-wlan0
ip_address=""
connection_name=wifi-p2p-p2p-dev-wlan0

wpa_cli -i p2p-dev-wlan0 p2p_stop_find
wpa_cli -i p2p-dev-wlan0 set config_methods virtual_push_button
wpa_cli -i p2p-dev-wlan0 p2p_find

# Wait and get the NDEF file.
dir_name=$(realpath $(dirname $0)/../)
ndef_file=$dir_name/build/ndef_file.bin
check_date=$(date +%Y-%m-%d\ %H:%M:%S)
connection_request=
ndef_sha256=
current_ndef_sha256=

# Write NDEF to optiga NBT
$dir_name/build/nbt-rpi -write $ndef_file > /dev/null 2>&1
if [[ $? -ne 0 ]]; then
    echo "Failed to write NDEF message"
    exit 1
fi

# Update the ndef hash
ndef_sha256=$(sha256sum $ndef_file | cut -d" " -f1)

# Wait till the android sends the connection request. Monitor in the journalctl for connection request
# Also verify that the NDEF message that is read is changed.
echo "Waiting for connection requests..."
while [[ $connection_request == "" ]]; do
    sleep 1
    connection_request=$(journalctl -u wpa_supplicant --since "$check_date" | grep "P2P-PROV-DISC-PBC-REQ\|P2P-INVITATION-RECEIVED\|P2P-DEVICE-FOUND")

    # journalctl -u wpa_supplicant --since "$check_date"
    check_date=$(date +%Y-%m-%d\ %H:%M:%S)

done


# Based on the request type get parse the SSID.
case "$connection_request" in
    *"P2P-PROV-DISC-PBC-REQ"*) 
        conn_request_mac=$(echo $connection_request | grep -oP "(?<=p2p_dev_addr=).*?(?=\ )")
        conn_request_ssid=$(echo $connection_request | grep -oP "(?<=name=').*?(?=')")
        ;;
    *"P2P-DEVICE-FOUND"*)
        conn_request_mac=$(echo $connection_request | grep -oP "(?<=p2p_dev_addr=).*?(?=\ )")
        conn_request_ssid=$(echo $connection_request | grep -oP "(?<=name=').*?(?=')")
        ;;
    *"P2P-INVITATION-RECEIVED"*)
        conn_request_mac=$(echo $connection_request | grep -oP "(?<=sa=).*?(?=\ )")
        ;;
    *)
        echo "Something went wrong!!"
        ;;
esac    

echo "Retrieved MAC address: $conn_request_mac"

if [[ $conn_request_ssid == "" ]]; then
    echo "SSID from the request not found, parsing it from p2p_peer command." 
    # Try to get p2p device name from p2p_peer
    conn_request_ssid=$(wpa_cli -i p2p-dev-wlan0 p2p_peer $conn_request_mac | grep device_name= | cut -d"=" -f2)
fi

echo "Connection Request from SSID: $conn_request_ssid, MAC: $conn_request_mac"

# Wait till the NDEF message in the OPTIGA NBT is changed
current_ndef_sha256=$(sha256sum $ndef_file | cut -d" " -f1)
while [[ $current_ndef_sha256 == $ndef_sha256 ]]; do
    # Read the NDEF file from OPTIGA NBT
    $dir_name/build/nbt-rpi -read $ndef_file > /dev/null  2>&1
    if [[ $? -ne 0 ]]; then
        echo "Failed to read NDEF message"
        exit 1
    fi
    current_ndef_sha256=$(sha256sum $ndef_file | cut -d" " -f1)
    sleep 1
done

# Get the SSID
ssid=$(python3 $dir_name/scripts/read_NDEF_message.py --file $ndef_file)
echo "SSID from NDEF message: $ssid"

if [ $? -ne 0 ]; then
    echo "Reading SSID from NDEF failed"
    exit 2
fi

# Check if the SSID from NDEF file and the one from wpa_supplicant connection request matches.
# i.e. The same device requested for connection estabishment.
if [[ "$conn_request_ssid" != "$ssid" ]]; then
    echo "The SSID of requested peer and the SSID in the NDEF messge did not match!"
    exit 1
fi

echo "Searching for device with SSID: $ssid"

# Wait till the device with ssid is available in the peers
mac_address=

peers=$(wpa_cli -i p2p-dev-wlan0 p2p_peers)
while [[ $mac_address == "" ]]; do
    peers=$(wpa_cli -i p2p-dev-wlan0 p2p_peers)
    len_peers=$(echo $peers | wc -w)
    for i in $peers; do 
        device_name=$(wpa_cli -i p2p-dev-wlan0 p2p_peer $i | grep device_name= | cut -d"=" -f2)
        if [[ $device_name == $ssid ]]; then
            echo "Device $device_name found. MAC: $i"
            mac_address=$i
            break
        fi
    done
done

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
