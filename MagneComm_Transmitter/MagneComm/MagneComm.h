#pragma once
#ifndef MagneComm_hpp
#define MagneComm_hpp

#include <iostream>
#include <string>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <sys/types.h>
#include <conio.h>
#include <Windows.h>
#include <mmsystem.h>
#include <vector>
#include <fstream>

#define __USE_GNU
#include<sched.h>
#define HAVE_STRUCT_TIMESPEC  
#include <pthread.h>

#pragma comment(lib, "pthreadVC2.lib")

//#define CPU_TOTAL_CORE_NUM 16

#define CPU_CORE_TX_NUM 4 // 2bit for pulse amplitude modulation
#define SYMBOL_BIT_NUM 4  // another 2bit for pulse width modulation
#define SYMBOL_TIME_LENGTH 5000 // ms
#define PACKAGE_TOTAL_BIT_NUM 20
#define PACKAGE_SYMBOL_NUM  5

#define HIGH_SPEED_CPU_CORE_TX_NUM 1 // 1bit for on-off-key
#define HIGH_SPEED_SYMBOL_TIME_LENGTH 500 // us
#define HIGH_SPEED_PREAMBLE_PER_LENGTH 1000// us

#define HIGH_SPEED_PACKAGE_TOTAL_BIT_NUM 2040
#define HIGH_SPEED_PACKAGE_SYMBOL_NUM_4BIT 510

typedef struct thread_data_struct {
	int  thread_id;
	DWORD time_window_length;  // ms
	DWORD pulse_width_value;   // %
	DWORD pulse_amplitude_value;  // int
	DWORD_PTR cpu_affinity_mask; // 0x00000001
}thread_data_struct;

typedef struct pwam_symbol_struct {
	DWORD pulse_width_value;
	DWORD pulse_amplitude_value;
	double* cpu_ideal_usage;
}pwam_symbol_struct;



class MagneComm
{
public:
	// Call the csharp dll to obtain the accurate CPU core usages
	int ObtainCPUInfo();
	int ObtainCPUCoreUsage();

	static void* OneCorePWMGenerator(void* thread_arg);
	void PWAMGenerator(DWORD window_length, DWORD width_value, DWORD amplitude_value);
	void PreambleGenerator(DWORD window_length);
	pwam_symbol_struct* ModulateData(int* package_data);
	int ReTransmitCheck(double* cpu_core_tx_usage, double* cpu_core_ideal_usage);

	int TransmitData(char* file_path);

	static pwam_symbol_struct* HighSpeedModulateData(int* package_data);

	static void* HighSpeedPWMGenerator(void* thread_arg);
	static void* HighSpeedTransmitControl(void* thread_arg);

	int HighSpeedTransmitData(char* file_path);

private:
	long cpu_init_flag = 0;
	double* cpu_core_usage;

	int txfile_data[30000];
};

#endif /* MagneComm_hpp */