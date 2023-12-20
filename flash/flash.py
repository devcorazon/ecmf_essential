import os
import sys
import subprocess
import re

def run_command(command):
    """Run a shell command and check if it was successful."""
    try:
        result = subprocess.call(command, shell=True)
        return result == 0
    except Exception as e:
        print(f"Error running command '{command}': {e}")
        return False

ESP_DEVICE = "COM9"  # Adjust this to your actual ESP device
FIRMWARE_BIN_FILE = "ecmf_essential.bin"  # Path to your firmware binary file
SERIAL_FILE = "serial_number.txt"  # File that contains the serial number
KEY_FILE = "efuse_key.txt"  # File that contains the key

ESPTOOL_PATH = "C:\\Users\\youcef.benakmoume\\esp-idf-v5.0.1\\components\\esptool_py\\esptool\\esptool.py"
ESPEFUSE_PATH = "C:\\Users\\youcef.benakmoume\\esp-idf-v5.0.1\\components\\esptool_py\\esptool\\espefuse.py"

SCRIPT_PATH = os.path.dirname(os.path.realpath(__file__))
SERIAL_FILE_FULL = os.path.join(SCRIPT_PATH, SERIAL_FILE)

# Read the hexadecimal serial number from the file
try:
    with open(SERIAL_FILE_FULL, 'r') as file:
        hex_number = file.read().strip()
except Exception as e:
    print(f"Error reading serial number file: {e}")
    sys.exit(1)

# Convert hex to binary string and pad it to be 32 bits
binary_number = bin(int(hex_number, 16))[2:].zfill(32)

# Determine which bits to burn
bits_to_burn = [i for i, bit in enumerate(reversed(binary_number)) if bit == '1']

print("Burning Serial Number...")

if not run_command(f'python {ESPEFUSE_PATH} --port {ESP_DEVICE} burn_bit BLOCK3 --force-write-always ' + ' '.join(map(str, bits_to_burn))):
    print("Error: Failed to burn eFuse bits for Serial Number.")
    sys.exit(1)

# Read the key from the file
try:
    with open(os.path.join(SCRIPT_PATH, KEY_FILE), 'r') as file:
        key_hex = file.read().strip()
except Exception as e:
    print(f"Error reading key file: {e}")
    sys.exit(1)

# Convert key hex to binary string and pad it
key_binary = bin(int(key_hex, 16))[2:].zfill(32)

# Determine which bits to burn for the key
key_bits_to_burn = [i for i, bit in enumerate(reversed(key_binary)) if bit == '1']

print("Burning Key to eFuse BLOCK5...")

# Execute the command for burning bits for the key
if not run_command(f'python {ESPEFUSE_PATH} --port {ESP_DEVICE} burn_bit BLOCK5 --force-write-always ' + ' '.join(map(str, key_bits_to_burn))):
    print("Error: Failed to burn eFuse bits for Key.")
    sys.exit(1)

        
print("Flashing firmware and updating serial number...")

# Flash the firmware binary
if not run_command(f'python {ESPTOOL_PATH} --port {ESP_DEVICE} --baud 1152000 --chip esp32c3 write_flash 0x0 bootloader.bin 0x8000 partition-table.bin 0x10000 {FIRMWARE_BIN_FILE}'):
    print("Error: Failed to flash firmware.")
    sys.exit(1)

# Increment the serial number and write it back to the file
NEW_SERIAL = "{:08X}".format(int(hex_number, 16) + 1)

try:
    with open(SERIAL_FILE_FULL, 'w') as file:
        file.write(NEW_SERIAL)
except Exception as e:
    print(f"Error writing back the new serial number: {e}")
    sys.exit(1)

print("Serial number updated and eFuse Key burned successfully! and finally Firmware updated")
