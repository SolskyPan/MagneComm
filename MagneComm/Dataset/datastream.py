# -*- coding: utf-8 -*-
"""
Created on Thu Oct 21 10:33:35 2021

@author: Lab
"""

import numpy as np
import random

txtpath = './Tx_highspeed.txt'
txtfile = open(txtpath, 'w')

data_count = 1000

for i in range(data_count):
    if(random.randint(0,99) % 2 == 0):
        txtfile.write('1')
    else:
        txtfile.write('0')
    if(i != data_count-1):
        txtfile.write('\n')
        
txtfile.close()
        