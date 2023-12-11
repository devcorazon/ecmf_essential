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

ESP_DEVICE = "COM7"  # Adjust this to your actual ESP device
FIRMWARE_BIN_FILE = "ecmf_essential.bin"  # Path to your firmware binary file
SERIAL_FILE = "serial_number.txt"  # File that contains the serial number

ESPTOOL_PATH = "C:\\Users\\youcef.benakmoume\\esp-idf-v5.0.1\\components\\esptool_py\\esptool\\esptool.py"
ESPEFUSE_PATH = "C:\\Users\\youcef.benakmoume\\esp-idf-v5.0.1\\components\\esptool_py\\esptool\\espefuse.py"

SCRIPT_PATH = os.path.dirname(os.path.realpath(__file__))
SERIAL_FILE_FULL = os.path.join(SCRIPT_PATH, SERIAL_FILE)

# Read the hexadecimal number from the file
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

# Construct the command for burning bits
burn_command = f'python {ESPEFUSE_PATH} --port {ESP_DEVICE} burn_bit BLOCK3 ' + ' '.join(map(str, bits_to_burn))

print("Flashing firmware and updating serial number...")

# Flash the firmware binary
if not run_command(f'python {ESPTOOL_PATH} --port {ESP_DEVICE} --baud 1152000 --chip esp32c3 write_flash 0x0 bootloader.bin 0x8000 partition-table.bin 0x10000 {FIRMWARE_BIN_FILE}'):
    print("Error: Failed to flash firmware.")
    sys.exit(1)

print("Burning bits to eFuse...")
if not run_command(burn_command):
    print("Error: Failed to burn eFuse bits.")
    sys.exit(1)

# Increment the serial number and write it back to the file
NEW_SERIAL = "{:08X}".format(int(hex_number, 16) + 1)

try:
    with open(SERIAL_FILE_FULL, 'w') as file:
        file.write(NEW_SERIAL)
except Exception as e:
    print(f"Error writing back the new serial number: {e}")
    sys.exit(1)

print("Serial number updated and eFuse bits burned successfully!")
