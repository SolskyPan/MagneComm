//
//  main.cpp
//
//  Created by Hao Pan on 2021. 10. 11.
//  Copyright © 2021 Hao Pan. All rights reserved.
//

#include <iostream>
#include "MagneComm.h"

int main() {
	std::cout << "Hello World.\n" << std::endl;

	HRESULT hr = CoInitialize(NULL);

	MagneComm magnecomm;

	char data_name[1024], data_highspeed_name[1024], file_format[1024];
	sprintf_s(data_name, "%s", "Tx1");
	sprintf_s(data_highspeed_name, "%s", "Tx_highspeed_bitstream");
	sprintf_s(file_format, "%s", "txt");

	char data_fullpath[1024], data_highspeed_fullpath[1024];
	sprintf_s(data_fullpath, "E:/MagneComm+/MagneComm_Transmitter/MagneComm/Dataset/%s.%s", data_name, file_format);
	sprintf_s(data_highspeed_fullpath, "E:/MagneComm+/MagneComm_Transmitter/MagneComm/Dataset/%s.%s", data_highspeed_name, file_format);

	int init_flag = 0;
	init_flag = magnecomm.ObtainCPUInfo(); //obtain cpu core num and freq

	Sleep(5000);
	std::cout << "Start Transmission.\n" << std::endl;
	if (init_flag == 1) {
		//magnecomm.TransmitData(data_fullpath);
		magnecomm.HighSpeedTransmitData(data_highspeed_fullpath);
	}


	std::cout << "Done" << std::endl;

	CoUninitialize();

	return 0;
}
