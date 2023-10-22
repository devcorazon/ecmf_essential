TO SIGN Firmware:

install open ssl 
https://slproweb.com/products/Win32OpenSSL.html
installl 
https://www.vim.org/download.php#pc

and add to environnement
C:\Program Files\OpenSSL-Win64\bin

sign the binaray like this 
espsecure.py sign_data --key secure_boot_signing_key.pem --version 2 --output build\ecmf_essential_signed.bin build\ecmf_essential.bin

Genrating key! 1
openssl genpkey -algorithm RSA -out secure_boot_signing_key_3072.pem -pkeyopt rsa_keygen_bits:3072

Generate public key and private key using  2
openssl genpkey -algorithm RSA -out private_key.pem
openssl rsa -pubout -in private_key.pem -out public_key.pem

how to FLASH entiere systeme

Full Erase: First, erase the flash completely:
esptool.py --chip esp32c3 --port COM7 --baud 921600 erase_flash

Flash Bootloader:
esptool.py --chip esp32c3 --port COM7 --baud 921600 write_flash 0x0 build/bootloader/bootloader.bin

Flash Partition Table:
esptool.py --chip esp32c3 --port COM7 --baud 921600 write_flash 0xa000 build/partition_table/partition-table.bin

Flash Application:
esptool.py --chip esp32c3 --port COM7 --baud 921600 write_flash 0x20000 build/ecmf_essential.bin