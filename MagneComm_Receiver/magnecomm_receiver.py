# -*- coding: utf-8 -*-
"""
MagneComm Receiver Implementation
@author: Hao Pan
"""

import numpy as np
import matplotlib.pyplot as plt
import matplotlib.lines as mlines
from scipy.ndimage import filters
from sklearn import preprocessing
from scipy.spatial.distance import euclidean
from fastdtw import fastdtw
from reedsolo import RSCodec

def minFilter(data, filter_length):
    filter_data = []
    for i in range(len(data) - filter_length):
        filter_data.append(min(data[i:i+filter_length]))        
    return filter_data

def readReceiveEMSignal(path, sample_rate, ms_sample_rate, filter_length,):
    rx_data = open(path).readlines()
    data = list(map(float, rx_data))
    data_flip = [0.0-item for item in data]
    sampling_interval = sample_rate // (ms_sample_rate*1000)
    downsample = data_flip[:: sampling_interval]
    filtered = minFilter(downsample, 5)
    
    # Min_Max Normalization
    filtered_ = np.array(filtered).reshape(-1,1)
    min_max_scaler = preprocessing.MinMaxScaler()
    normalized = min_max_scaler.fit_transform(filtered_)
    normalized = normalized.reshape(1,-1).tolist()[0]
    
    return normalized

def rmse(x, y):
    return np.sqrt(((x - y) ** 2).mean())

def euclid_dist(x,y):
    return np.sqrt(sum((x-y)**2))

def dtw_fast(x, y):
    x_reshaped = x.reshape(-1, 1)
    y_reshaped = y.reshape(-1, 1)
    distance, path = fastdtw(x_reshaped, y_reshaped, dist=euclidean)
    return distance

def detect_symbol(x, threshold = 0.2):
    start_temp_list = []
    diff_value_list = []
    for i in range(len(x)-1):
        if(x[i+1] - x[i] > threshold):
            start_temp_list.append(i)
            diff_value_list.append(x[i+1] - x[i])
    
    if(len(start_temp_list) == 0):
        return -1
    else:
        return start_temp_list[diff_value_list.index(max(diff_value_list))]
        
def decode_PWAM(x, range_list):
    amplitude = 0
    width = 0
    
    width_range = [[1, 28], [28, 46], [46, 68], [68, 99]]
    level_count = [0,0,0,0,0]
    
    for i in range(len(x)):
        if( x[i] >= range_list[0][0]) and (x[i] < range_list[0][1]):
            level_count[0] += 1
        if( x[i] >= range_list[1][0]) and (x[i] < range_list[1][1]):
            level_count[1] += 1
        if( x[i] >= range_list[2][0]) and (x[i] < range_list[2][1]):
            level_count[2] += 1
        if( x[i] >= range_list[3][0]) and (x[i] < range_list[3][1]):
            level_count[3] += 1
        if( x[i] >= range_list[4][0]) and (x[i] < range_list[4][1]):
            level_count[4] += 1

    for i in range(5): 
        temp_width = level_count[4-i] / len(x)
        if(temp_width > 0.05):
            amplitude = 4 - i
            if(temp_width >= width_range[0][0]/100) and (temp_width < width_range[0][1]/100):
                width = 20
            if(temp_width >= width_range[1][0]/100) and (temp_width < width_range[1][1]/100):
                width = 40
            if(temp_width >= width_range[2][0]/100) and (temp_width < width_range[2][1]/100):
                width = 60
            if(temp_width >= width_range[3][0]/100) and (temp_width <= width_range[3][1]/100):
                width = 80           
            break
        else:
            continue
    
    return amplitude, width, temp_width
    

def transferBit(amplitude, width):
    bit_stream = ""
    if(amplitude == 0) or (amplitude == 1):
        bit_stream += "00"
    elif(amplitude == 2):
        bit_stream += "01"
    elif(amplitude == 3):
        bit_stream += "10"
    elif(amplitude == 4):
        bit_stream += "11"
        
    if(width == 20):
        bit_stream += "00"
    elif(width == 40):
        bit_stream += "01"
    elif(width == 60):
        bit_stream += "10"
    elif(width == 80):
        bit_stream += "11"
        
    return bit_stream

def detectPreamble(em_signal, pattern, downsample_rate, preamble_length = 1, package_length = 270):
    print("Start detect the preamble...")
    preamble_pattern = np.array(pattern)*0.25
    preamble_examplar = []
    for i in range(len(preamble_pattern)):
        for j in range(preamble_length* downsample_rate):
            preamble_examplar.append(preamble_pattern[i])
    examplar = np.array(preamble_examplar)
    preamble_point_length = len(preamble_examplar)

    start_time_index = []
    stop_time_index = []
    search_length = 20
    for i in range(6):
        start_time_index.append(7 + package_length*i)
        stop_time_index.append(7 + search_length + package_length*i)

    preamble_startpoint_alg1 = []
    preamble_startpoint_alg2 = []
    preamble_startpoint_alg3 = []
    
    preamble_startpoint = []

    for package_i in range(0, 6):
        start_time = start_time_index[package_i] *downsample_rate
        end_time = stop_time_index[package_i] *downsample_rate
        
        fig, axs = plt.subplots(1, 1, figsize=(18, 6), sharex=True)
        fig.set_tight_layout({'pad': 1})
        
        axs.set_title("Package: " + str(package_i))
        axs.plot(em_signal[start_time:end_time]) #[::2]
        
        distance_list1 = []
        distance_list2 = []
        distance_list3 = []
        
        for i in range(start_time, end_time):
            instance = np.array(em_signal[i:i+preamble_point_length])
            distance_list1.append(dtw_fast(examplar, instance))
            distance_list2.append(euclid_dist(examplar, instance))
            distance_list3.append(rmse(examplar, instance))
        
        min_index1 = distance_list1.index(min(distance_list1))
        preamble_startpoint_alg1.append(min_index1)
        
        min_index2 = distance_list2.index(min(distance_list2))
        preamble_startpoint_alg2.append(min_index2)
        
        min_index3 = distance_list3.index(min(distance_list3))
        preamble_startpoint_alg3.append(min_index3)
        
        ymin = 0
        ymax = 1
        xmin1 = min_index1
        xmax1 = xmin1
        l = mlines.Line2D([xmin1,xmax1], [ymin,ymax], color = 'red')
        axs.add_line(l)
        
        xmin2 = min_index2
        xmax2 = xmin2
        l = mlines.Line2D([xmin2,xmax2], [ymin,ymax], color = 'blue')
        axs.add_line(l)
        
        xmin3 = min_index3
        xmax3 = xmin3
        l = mlines.Line2D([xmin3,xmax3], [ymin,ymax], color = 'green')
        axs.add_line(l)
        
    preamble_start_final = []
    preamble_start_final_point = []
    for i in range(len(preamble_startpoint_alg1)):
        p_start_temp = [preamble_startpoint_alg1[i], preamble_startpoint_alg2[i], preamble_startpoint_alg3[i]]
        counts = np.bincount(p_start_temp)  
        preamble_start_final.append(np.argmax(counts))
        preamble_start_final_point.append((7 + package_length*i)*100 + np.argmax(counts))
            
        
    print("Preamble patterns have been detected!")
    return preamble_start_final_point
    # return preamble_startpoint_alg1, preamble_startpoint_alg2, preamble_startpoint_alg3, preamble_start_final, preamble_start_final_index


def detectSymbol(em_signal, symbol_num, package_index, preamble_start_index):
    preamble_startpoint = np.array(preamble_start_index)
    preamble_stoppoint = preamble_startpoint + 6*100 #[0,3,1,2,4,0] 6 ms
    
    package_symbol_emdata = em_signal[preamble_stoppoint[package_index]: preamble_startpoint[package_index+1]]

    symbol_start_point = []
    last_symbol_start_point = 0
    
    symbol_start_list = [0]
    
    find_area = 8
    for i in range(symbol_num- 1):
        current_symbol_data = package_symbol_emdata[last_symbol_start_point+ 50 -find_area: last_symbol_start_point+ 50 + find_area]
        symbol_start_point = detect_symbol(current_symbol_data, 0.12)
        if (symbol_start_point == -1): # didn't find symbol
            symbol_start_point = find_area
            symbol_current_point = symbol_start_point + last_symbol_start_point + 50 -find_area
            find_area = 20
        else:
            symbol_current_point = symbol_start_point + last_symbol_start_point + 50 -find_area
            find_area = 8
        symbol_start_list.append(symbol_current_point)
        last_symbol_start_point = symbol_current_point #last_symbol_start_point + symbol_start_point + 50 -find_area

    return symbol_start_list


def decodePWAMSymbol(em_signal, preamble_start_index, package_index, symbol_start_list):
    preamble_startpoint = np.array(preamble_start_index)
    preamble_stoppoint = preamble_startpoint + 600
    
    ### Channel estimation
    preamble_pattern = em_signal[preamble_startpoint[1] : preamble_stoppoint[2]]

    amplitude_level = []
    amplitude_level.append(( np.mean(preamble_pattern[:100]) + np.mean(preamble_pattern[-100:]) ) / 2) #amplitude_level_0
    amplitude_level.append(np.mean(preamble_pattern[200:300])) #1
    amplitude_level.append(np.mean(preamble_pattern[300:400])) #2
    amplitude_level.append(np.mean(preamble_pattern[100:200])) #3
    amplitude_level.append(np.mean(preamble_pattern[400:500])) #4
    
    amplitude_level_range = []
    for i in range(len(amplitude_level)):
        if(i == 0):
            start_point = 0.0
            end_point = amplitude_level[i] + (amplitude_level[i+1] - amplitude_level[i]) / 2
        elif(i == 4):
            start_point = amplitude_level[i] - (amplitude_level[i] - amplitude_level[i-1]) / 2
            end_point = 1.0
        else:
            start_point = amplitude_level[i] - (amplitude_level[i] - amplitude_level[i-1]) / 2
            end_point = amplitude_level[i] + (amplitude_level[i+1] - amplitude_level[i]) / 2
    
        amplitude_level_range.append([start_point, end_point])


    package_symbol_data = em_signal[preamble_stoppoint[package_index]:preamble_startpoint[package_index+1]]
    
    bit_stream = ""
    for i in range(510):
        if(i == 509):
            symbol_data = package_symbol_data[symbol_start_list[i]:-1]
        else:
            symbol_data = package_symbol_data[symbol_start_list[i]:symbol_start_list[i+1]]
            
        symbol_data = np.array(symbol_data)
        amplitude, width, temp_width = decode_PWAM(symbol_data, amplitude_level_range)
        bit_stream += transferBit(amplitude, width)
    
    return bit_stream



def RSDecodeBitStream(bit_stream, rs_paras = 180):
    hex_string = hex(int(bit_stream, 2))[2:]
    if(len(hex_string) % 2 == 0):
        hex_string = '00' + hex_string
    else:
        hex_string = '0' + hex_string
        
    received_bytes = bytes.fromhex(hex_string)
    rsc = RSCodec(rs_paras)
    content = rsc.decode(received_bytes)
    return content
    


sample_rate = 1000000
ms_sample_rate = 100
preamble_pattern = [0,3,1,2,4,0]
data_path = './receiver_highspeed_communication_example.txt'
em_data = readReceiveEMSignal(data_path, sample_rate, ms_sample_rate, 5)
    

# p_start_point_package = detectPreamble(em_data, preamble_pattern, 100, 1, 270)
p_start_point_package = np.loadtxt('./preamble_start_point.txt').astype(int)

symbol_num = 255*8//4
string_content = bytearray("".encode())

for package_index in range(5):
    symbol_index_package = detectSymbol(em_data, symbol_num, package_index, p_start_point_package)
    bitstream = decodePWAMSymbol(em_data, p_start_point_package, package_index, symbol_index_package)
    try:
        rx_content = RSDecodeBitStream(bitstream)
        string_content += rx_content[0][1:]
    except:
        pass
    
print(string_content)

















# np.savetxt('./preamble_start_index.txt', preamble_startpoint)


# draw_data = em_data[5*downsample_rate:1500*downsample_rate]
# fig, axs = plt.subplots(1, 1, figsize=(18, 6), sharex=True)
# axs.plot(draw_data)


# start_time_index = []
# stop_time_index = []
# for i in range(10):
#     start_time_index.append(5+ 267*i)
#     stop_time_index.append(25+ 267*i)
    





# preamble_startpoint_relative = np.loadtxt('./preamble_start_index_final.txt').astype(int)
# preamble_startpoint = []
# preamble_stoppoint = []
# for i in range(10):   
#     preamble_startpoint.append(start_time_index[i]*100 + preamble_startpoint_relative[i])
#     preamble_stoppoint.append(start_time_index[i]*100 + preamble_startpoint_relative[i] + 600)
    


######### Decode the PWAM symbol
"""
preamble_pattern = X_normal[preamble_startpoint[0] : preamble_stoppoint[1]]

amplitude_level = []
amplitude_level.append(( np.mean(preamble_pattern[:100]) + np.mean(preamble_pattern[-100:]) ) / 2) #amplitude_level_0
amplitude_level.append(np.mean(preamble_pattern[200:300])) #1
amplitude_level.append(np.mean(preamble_pattern[300:400])) #2
amplitude_level.append(np.mean(preamble_pattern[100:200])) #3
amplitude_level.append(np.mean(preamble_pattern[400:500])) #4


amplitude_level_range = []
for i in range(len(amplitude_level)):
    if(i == 0):
        start_point = 0.0
        end_point = amplitude_level[i] + (amplitude_level[i+1] - amplitude_level[i]) / 2
    elif(i == 4):
        start_point = amplitude_level[i] - (amplitude_level[i] - amplitude_level[i-1]) / 2
        end_point = 1.0
    else:
        start_point = amplitude_level[i] - (amplitude_level[i] - amplitude_level[i-1]) / 2
        end_point = amplitude_level[i] + (amplitude_level[i+1] - amplitude_level[i]) / 2

    amplitude_level_range.append([start_point, end_point])
    


package_num = 3
symbol_start_list = np.loadtxt('./symbol_index_package' + str(package_num) + '.txt').astype(int)

package_symbol_data = X_normal[preamble_stoppoint[package_num]:preamble_startpoint[package_num+1]]
# package_symbol_data = X_normal[preamble_stoppoint[9]:preamble_stoppoint[9]+26150]


fig, axs = plt.subplots(1, 1, figsize=(18, 6), sharex=True)
axs.plot(package_symbol_data)

amplitude_list_show = []
width_list_show = []
temp_width_show = []


bit_stream = ""
for i in range(510):
    if(i == 509):
        symbol_data = package_symbol_data[symbol_start_list[i]:-1]
    else:
        symbol_data = package_symbol_data[symbol_start_list[i]:symbol_start_list[i+1]]
    amplitude, width, temp_width = decode_PWAM(symbol_data, amplitude_level_range)
    
    bit_stream += transferBit(amplitude, width)
    
    # axs.text(symbol_start_list[i], 0.9, str(amplitude)+"_"+str(width) +"\n"+str(int(temp_width*100)), style ='italic', fontsize = 10, color ="red") 
    # print ("Symbol "+ str(i) + ": ----------- ", amplitude, width)
    amplitude_list_show.append(amplitude)
    width_list_show.append(width)
    temp_width_show.append(temp_width)


print(amplitude_list_show)

print(width_list_show)

"""



"""
### Detect the symbol index point
fig, axs = plt.subplots(1, 1, figsize=(18, 6), sharex=True)
axs.plot(X_normal[preamble_startpoint[9]:preamble_stoppoint[9]+26150])
# axs.plot(package_symbol_data)

symbol_num = 510


symbol_start_point = []
last_symbol_start_point = 0

ymin = 0
ymax = 1
xmin = 0
xmax = 0
l = mlines.Line2D([xmin,xmax], [ymin,ymax], color = 'red')
axs.add_line(l)


symbol_start_list = [0]


find_area = 7
for i in range(symbol_num- 1):
    current_symbol_data = package_symbol_data[last_symbol_start_point+ 50 -find_area: last_symbol_start_point+ 50 + find_area]
    symbol_start_point = detect_symbol(current_symbol_data, 0.12)
    if (symbol_start_point == -1): # didn't find symbol
        symbol_start_point = find_area
        xmin = symbol_start_point + last_symbol_start_point + 50 -find_area
        xmax = xmin
        
        ymin = 0
        ymax = 1
        l = mlines.Line2D([xmin,xmax], [ymin,ymax], color = 'green')
        axs.add_line(l)
        
        find_area = 15
    else:
        xmin = symbol_start_point + last_symbol_start_point + 50 -find_area
        xmax = xmin
        
        ymin = 0
        ymax = 1
        l = mlines.Line2D([xmin,xmax], [ymin,ymax], color = 'red')
        axs.add_line(l)
        
        find_area = 7
        
    
    symbol_start_list.append(xmin)
        
    

    last_symbol_start_point = xmin #last_symbol_start_point + symbol_start_point + 50 -find_area
"""




"""
#### detect the preamble

"""


# symbol_num = 0.1 * sampling_rate

# for j in range(100):
#     xmin = j * symbol_num + 5 * sampling_rate + 500
#     xmax = xmin
#     l = mlines.Line2D([xmin,xmax], [ymin,ymax], color = 'green')
#     axs.add_line(l)



# preamble_one_num = 1*sampling_rate
# for i in range(6):
#     xmin = i * preamble_one_num
#     xmax = i * preamble_one_num

#     l = mlines.Line2D([xmin,xmax], [ymin,ymax], color = 'red')
#     axs.add_line(l)
    
    
#     xmin1 = i * preamble_one_num + 15*sampling_rate
#     xmax1 = i * preamble_one_num + 15*sampling_rate
#     l1 = mlines.Line2D([xmin1,xmax1], [ymin,ymax], color = 'red')
    
#     axs.add_line(l1)



