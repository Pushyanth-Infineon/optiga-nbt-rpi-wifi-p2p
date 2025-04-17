import ndef
import hashlib

def hex_to_array(data: bytes) -> str:
    print("{", end = "")
    for i in range(len(data)):
        print(f"0x{data[i]:02x}", end = "")
        if ((i + 1) != len(data)):
            print(", ", end = "")
    print("}")

pkhash = hashlib.sha256(b'DUMMY').digest()[0:20]
oobpwd = ndef.wifi.OutOfBandPassword(pkhash, 0x0007, b'')
wfaext = ndef.wifi.WifiAllianceVendorExtension(('version-2', b'\x20'))
carrier = ndef.WifiSimpleConfigRecord()
carrier.name = '0'
SSID = b'DIRECT-RasPi1'
mac_address = bytes(bytearray([0xDE, 0xA6, 0x32, 0xAA, 0X45, 0xBA]))
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
hex_to_array(octets)