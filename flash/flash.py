import os
import sys
import subprocess
from binascii import unhexlify, hexlify
import re

ESP_DEVICE = "COM9" # Adjust this to your actual ESP device
FIRMWARE_BIN_FILE = "ecocomfort_essential.bin" # Path to your firmware binary file
SERIAL_FILE = "serial_number.txt" # File that contains the serial number
BIN_FILE = "serial_number.bin" # Binary file to be created

ESPTOOL_PATH = "C:\\Users\\youcef.benakmoume\\esp-idf-v5.0.1\\components\\esptool_py\\esptool\\esptool.py"
ESPEFUSE_PATH = "C:\\Users\\youcef.benakmoume\\esp-idf-v5.0.1\\components\\esptool_py\\esptool\\espefuse.py"

SCRIPT_PATH = os.path.dirname(os.path.realpath(__file__))

SERIAL_FILE_FULL = os.path.join(SCRIPT_PATH, SERIAL_FILE)
BIN_FILE_FULL = os.path.join(SCRIPT_PATH, BIN_FILE)

with open(SERIAL_FILE_FULL, 'r') as file:
    CURRENT_SERIAL = file.read().strip().upper()

# Remove non-hexadecimal characters from CURRENT_SERIAL
CURRENT_SERIAL = re.sub('[^0-9A-Fa-f]', '', CURRENT_SERIAL)

print('CURRENT_SERIAL:', CURRENT_SERIAL)
BYTES = unhexlify(CURRENT_SERIAL)

if len(BYTES) > 4:
    print("Error: Byte representation of serial number is too large for eFuse block.")
    sys.exit(1)

with open(BIN_FILE_FULL, 'wb') as file:
    file.write(BYTES)

print("Flashing firmware and updating serial number...")

# Flash the firmware binary
subprocess.call(f'python {ESPTOOL_PATH} --port {ESP_DEVICE} --baud 1152000 --chip esp32c3 write_flash 0x0 bootloader.bin 0x8000 partition-table.bin 0x10000 {FIRMWARE_BIN_FILE}', shell=True)

# Write the serial number to eFuse
subprocess.call(f'python {ESPEFUSE_PATH} --port {ESP_DEVICE} burn_block_data --offset 28 BLOCK3 {BIN_FILE_FULL}', shell=True)

# Increment the serial number and write it back to the file
NEW_SERIAL = "{:08X}".format(int(CURRENT_SERIAL, 16) + 1)

with open(SERIAL_FILE_FULL, 'w') as file:
    file.write(NEW_SERIAL)

# Delete the bin file
os.remove(BIN_FILE_FULL)

print("Serial number updated and firmware flashed successfully!")
