# -*- coding: utf-8 -*-
"""
Transfer the string file to the bin stream, add the package id and RS code!
@author: Hao Pan
"""

import numpy as np
import random
from reedsolo import RSCodec


def packageInputData(txtpath, pack_num):
    txtfile = open(txtpath, 'rb')
    txt_bytes = txtfile.read(2560)
    
    pack_bytes_num = len(txt_bytes) // pack_num
    pack_bytes_array = []
    for i in range(pack_num):
        temp_array = bytearray(txt_bytes[i* pack_bytes_num: (i+1)* pack_bytes_num])
        # add the package id (1 bytes) into per package
        temp_array.insert(0, i)
        
        pack_bytes_array.append(temp_array)
        
    txtfile.close()
        
    return pack_bytes_array
    

def rsCodingData(raw_bytes_array, error_bit_num):
    encoded_bytes = bytearray(b"")
    rsc = RSCodec(error_bit_num) # totally transmit 255 bit, $error_bit_num$ is the redundant data
    for i in range(len(raw_bytes_array)):        
        encode = rsc.encode(raw_bytes_array[i])
        encoded_bytes = encoded_bytes + encode
    return encoded_bytes


def generateTXBinStream(encoded_bytes, bit_file_path, write_file_flag = 0):
    bit_list = list()
    bit_lise_temp = list(bin(int(encoded_bytes.hex(), 16))[2:])
    
    t = len(encoded_bytes)*8 - len(bit_lise_temp) # complete bytes
    while t> 0:
        t-=1
        bit_lise_temp.insert(0, 0)
    bit_list.extend([int(x) for x in bit_lise_temp])
    
    if(write_file_flag):
        binfile = open(bit_file_path, 'w')
        for item in bit_list:
            binfile.write(str(item)+"\n")
        binfile.close()
        
    return bit_list

 
if __name__ == '__main__':
    txt_content_path = './Tx_string_content.txt'
    tx_bitstream_path = './Tx_highspeed_bitstream.txt'
    package_num = 5
    
    package_array = packageInputData(txt_content_path, package_num)
    rscoded_array = rsCodingData(package_array, 180) #255/120
    transmit_bitstream = generateTXBinStream(rscoded_array, tx_bitstream_path, 1)
    
    total_bit_num = len(transmit_bitstream)
    