"""
AVR-SBC .bmp Image Loader
Author: Balazs Markus

This Python program facilitates the loading of 128x64 binary .bmp images onto an AVR-SBC (Single Board Computer).
It involves converting the .bmp image into a binary format compatible with the AVR-SBC's display capabilities
and transferring it over a serial connection to the board.

The program can be started without any arguments, and during runtime the following two inputs have to be provided:
- COM Port: The COM port to use for the serial connection with 9600 baud (e.g., COM4).
- Input Filename: The filename of the input 128x64 binary .bmp image to load onto the AVR-SBC.

This tool is designed to facilitate the loading of 128x64 binary .bmp images onto an AVR-SBC (Single Board Computer). The process involves converting the .bmp image into a binary format compatible with the AVR-SBC's display capabilities and then transferring it over a serial connection to the board.

Here's how the process works:
1. Reading the .bmp File: The program reads the header and info header of the .bmp file to extract essential information about the image, such as its dimensions and color depth.
2. Converting to Binary: After verifying that the image is monochrome, the program calculates padding bytes and extracts the pixel data. Each pixel is represented as a bit, and these bits are then converted into bytes and written into a binary file with a .bin extension.
3. Sending Data to AVR-SBC: The binary file is sent to the AVR-SBC in four batches. Each batch consists of 256 bytes of image data. The program prompts you to enter "SERLOAD" on the AVR-SBC to initiate the data transfer.
4. Finalizing the Image: Once the four image data batches are sent, in the fifth, final batch, the program sends drawing commands to the AVR-SBC to display the loaded image on the screen.
5. Closing the Connection: Finally, the program closes the serial connection and exits.

Please follow the instructions provided by the program to load your desired image onto the AVR-SBC successfully.
"""

import serial
import serial.tools.list_ports
import struct
import numpy as np
import matplotlib.pyplot as plt
import os

# BMP file header structure
HEADER_SIZE = 14
header_format = '<2sIHHI'

# BMP info header structure
INFOHEADER_SIZE = 40
infoheader_format = '<IiiHHIIiiII'

def read_bmp(filename):
    """
    Read the specified .bmp file and convert it into a binary format.
    
    Args:
    - filename (str): The filename of the input .bmp image.
    
    Returns:
    - np.array: The binary pixel data of the image.
    """
    with open(filename, 'rb') as f:
        output_filename = os.path.splitext(filename)[0] + '.bin'  # Change extension to .bin
        if os.path.exists(output_filename):
            os.remove(output_filename)  # Delete the existing binary file
        # Read file header
        header = struct.unpack(header_format, f.read(HEADER_SIZE))
        print("Header:", header)

        # Read info header
        infoheader = struct.unpack(infoheader_format, f.read(INFOHEADER_SIZE))
        print("Info Header:", infoheader)

        # Check if the image is monochrome
        if infoheader[4] != 1:
            print("The image is not monochrome.")
            return

        # Calculate the number of padding bytes at the end of each row
        width, height = infoheader[1], infoheader[2]
        padding = ((width + 7) // 8) % 4

        # Skip to the start of the pixel data
        f.seek(header[4])

        # Read the image data, skipping the padding bytes
        data = []
        for _ in range(height):
            row = [format(b, '08b') for b in f.read((width + 7) // 8)]
            # Convert each byte to individual bits
            bits = [bit for byte in row for bit in byte]
            data.append(bits)
            #for individualbit in bits:
                #print(individualbit, end='')
            with open(output_filename, 'ab') as output_file:
                output_file.write(bytes(int(binary, 2) for binary in row))
            f.seek(padding, 1)  # Skip the padding bytes

        return np.array(data, dtype=np.uint8)

def plot_image(data):
    """
    Plot the image represented by the binary pixel data.
    
    Args:
    - data (np.array): The binary pixel data of the image.
    """
    plt.imshow(data)
    plt.show(block = False)

# Function to send XPoke command
def send_xpoke(byte_val, address):
    """
    Send XPOKE command to the AVR-SBC.
    
    Args:
    - byte_val (int): The byte value to send.
    - address (int): The memory address to send the byte value to.
    """
    line_number = address * 10
    command = f"{line_number} XPOKE {byte_val}, {address}\r\n"
    print(command.replace("\r\n",""))
    ser.write(command.encode())

# Function to send data in batches
def send_data_in_batches(data):
    """
    Send the binary image data to the AVR-SBC in batches.
    
    Args:
    - data (bytes): The binary image data to send.
    """
    for i in range(0, len(data), 256):
        input("Type SERLOAD on the AVR-SBC and press Enter to continue...")
        batch = data[i:i+256]
        for byte_index, byte_val in enumerate(batch):
            send_xpoke(byte_val, i + byte_index)
        ser.write(b'\x00')  # Terminating zero
        input("Type RUN on the AVR-SBC and press Enter to continue...")


# Welcome message
print("Welcome to the AVR-SBC .bmp Image Loader!\n")
print("This tool is designed to facilitate the loading of 128x64 binary .bmp images onto an AVR-SBC (Single Board Computer). The process involves converting the .bmp image into a binary format compatible with the AVR-SBC's display capabilities and then transferring it over a serial connection to the board.\n")

# Process steps
print("Here's how the process works:\n")

# Step 1: Reading the .bmp File
print("1. Reading the .bmp File: The program reads the header and info header of the .bmp file to extract essential information about the image, such as its dimensions and color depth.\n")

# Step 2: Converting to Binary
print("2. Converting to Binary: After verifying that the image is monochrome, the program calculates padding bytes and extracts the pixel data. Each pixel is represented as a bit, and these bits are then converted into bytes and written into a binary file with a .bin extension.\n")

# Step 3: Sending Data to AVR-SBC
print("3. Sending Data to AVR-SBC: The binary file is sent to the AVR-SBC in four batches. Each batch consists of 256 bytes of image data. The program prompts you to enter \"SERLOAD\" on the AVR-SBC to initiate the data transfer.\n")

# Step 4: Finalizing the Image
print("4. Finalizing the Image: Once the four image data batches are sent, in the fifth, final batch, the program sends drawing commands to the AVR-SBC to display the loaded image on the screen.\n")

# Step 5: Closing the Connection
print("5. Closing the Connection: Finally, the program closes the serial connection and exits.\n")

# Closing message
print("Please follow the instructions provided by the program to load your desired image onto the AVR-SBC successfully. Let's get started!\n")

# Get a list of available COM ports
available_ports = serial.tools.list_ports.comports()
print("Available COM ports:")
for port in available_ports:
    print(str(port.device))

input_comport = input("Enter which COM port to use: ")

# Attempt to open serial port
try:
    ser = serial.Serial(input_comport, 9600)
except serial.SerialException:
    print("Error: Could not open serial port. Please check if the COM port is correct.")
    input("Press Enter to exit...")
    exit()
    
input_filename = input("Enter the input filename (.bmp): ")

# Check if the input file exists
if not os.path.exists(input_filename):
    print("Error: Input file does not exist.")
    input("Press Enter to exit...")
    exit()

data = read_bmp(input_filename)
#if data is not None:
#    plot_image(data)
#    input("Press Enter to exit...")


input_filename = os.path.splitext(input_filename)[0] + '.bin'

# Read binary file
with open(input_filename, 'rb') as f:
    data = f.read()
    if len(data) != 1024:
        print("Error: Input file should be 1024 bytes large.")
    else:
        # Send data in batches
        send_data_in_batches(data)

input("Type SERLOAD on the AVR-SBC and press Enter to continue...")

# Send ending commands
ending_commands = [
    "5 CLS\r\n",
    "10 X=0\r\n",
    "20 Y=0\r\n",
    "30 FOR I=0 TO 1023\r\n",
    "40 XPEEK N,I\r\n",
    "50 D=128\r\n",
    "60 FOR J=0 TO 7\r\n",
    "70 B=N/D\r\n",
    "80 B=B-(B/2)*2\r\n",
    "90 DRAWPIX X+40,64-Y+80,B\r\n",
    "100 X=X+1\r\n",
    "110 IF X=128 THEN X=0:Y=Y+1\r\n",
    "120 D=D/2\r\n",
    "130 NEXT J\r\n",
    "140 NEXT I\r\n"
]

for command in ending_commands:
    print(command.replace("\r\n",""))
    ser.write(command.encode())

ser.write(b'\x00')  # Terminating zero

# Close serial port
ser.close()

print("Type RUN on the AVR-SBC")

input("Press Enter to exit...")
