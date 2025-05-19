import ndef
import hashlib
import re
import sys
import os
import subprocess
import argparse


# One option.
# 1. --no-overwrite (Do not overwrite the array in main.c, just prints the array)
parser = argparse.ArgumentParser(description="Process argument.")

# Add the --no-overwrite argument as a flag
parser.add_argument('--no-overwrite', action='store_true', 
                    help="Do not overwrite the array in main.c, just prints the array.")

# Parse the arguments
args = parser.parse_args()

def hex_to_array(data: bytes) -> str:
    array = "\n\t"
    for i in range(len(data)):
        array += f"0x{data[i]:02x}"
        if ((i + 1) != len(data)):
            array += ", "
        else:
            array += "\n"
        
        if ((i + 1) % 8) == 0:
            array += "\n\t"

    return array

pkhash = hashlib.sha256(b'DUMMY').digest()[0:20]
oobpwd = ndef.wifi.OutOfBandPassword(pkhash, 0x0007, b'')
wfaext = ndef.wifi.WifiAllianceVendorExtension(('version-2', b'\x20'))
carrier = ndef.WifiSimpleConfigRecord()
carrier.name = '0'
SSID = b'DIRECT-RasPi1'
mac_address: str = ""

# Get mac address using wpa_cli command.
# Command: wpa_cli -i p2p-dev-wlan0 status | grep "p2p_device_address" | cut -d"=" -f2"
try:
    process_wpa_cli = subprocess.Popen(["wpa_cli", "-i", "p2p-dev-wlan0", "status"], stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    output_wpa_cli, err_wpa_cli = process_wpa_cli.communicate()

    if (process_wpa_cli.returncode != 0):
        print(f"Error running wpa_cli command: {err_wpa_cli.decode()}")
    else:
        for line in output_wpa_cli.decode("utf-8").split():
            match = re.search(r"p2p_device_address=(.*)", line)
            if match:
                mac_address = match.group(1)
except Exception as e:
    print(e)

if mac_address == "":
    print("Could not retrieve p2p-dev-wlan0 MAC address")
    exit(1)


# Convert the mac address to byte array
mac_address: bytes = bytes(bytearray(map(lambda x: int(x, base=16), mac_address.split(":"))))
carrier.set_attribute('oob-password', oobpwd)
carrier.set_attribute('ssid', SSID)
carrier.set_attribute('rf-bands', '2.4GHz')
carrier.set_attribute('ap-channel', 6)
carrier.set_attribute('mac-address', mac_address)
carrier['vendor-extension'] = [wfaext.encode()]
hs = ndef.handover.HandoverSelectRecord('1.3')
hs.add_alternative_carrier('active', carrier.name)
octets = b''.join(ndef.message_encoder([hs, carrier]))

# Add length to the NDEF message
octets = bytes(bytearray([(len(octets) >> 8), (len(octets) & 0xFF)])) + octets
array_str = hex_to_array(octets)

if args.no_overwrite:
    print(array_str)
else:
    # Get the directory of this script 
    script_dir = os.path.dirname(sys.argv[0])
    with open(script_dir + "/../source/main.c", "r") as f:
        c_code = f.read()

        # Regex pattern to match the array definition
        pattern = r"(static\s+uint8_t\s+WIFI_CONNECTION_HANDOVER_MESSAGE\[\]\s*=\s*\{)(.*?)(\};)"

        # Replace the array values
        updated_code = re.sub(pattern, rf"\1{array_str}\3", c_code, flags=re.DOTALL)

        # Replace in main.c
        with open(script_dir + "/../source/main.c", "w") as f:
            f.write(updated_code)

