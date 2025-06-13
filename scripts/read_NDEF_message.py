# SPDX-FileCopyrightText: Copyright (c) 2025 Infineon Technologies AG
# SPDX-License-Identifier: MIT

import ndef
import argparse
import sys


def bytes_to_array(data: bytes) -> str:
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

parser = argparse.ArgumentParser(description="Process argument.")

# NDEF file (binary)
# 1. --file

parser.add_argument('--file', 
                    help="NDEF binary file path")

# Parse the arguments
args = parser.parse_args()

ndef_file = open(args.file, "rb")

# Ignore the length
ndef_file.read(2)
ndef_bytes: bytes = ndef_file.read()

# print(bytes_to_array(ndef_bytes), file=sys.stderr)
SSID = None

for record in ndef.message_decoder(ndef_bytes):
    if (type(record) == ndef.wifi.WifiSimpleConfigRecord):
        # Get the SSID
        SSID = record['ssid'][0].decode('utf-8')

if (SSID is not None):
    print(SSID)
else:
    print("SSID not found")
    exit(1)