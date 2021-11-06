//
//  MagneComm.cpp
//  MagneComm
//
//  Created by Hao Pan on 2021. 10. 11.
//  Copyright © 2021 Hao Pan. All rights reserved.
//

#include "MagneComm.h"

//#import "E:\magnecomm\test\ClassLibrary3\ClassLibrary3\bin\Debug\CPUCoreUsage.tlb" raw_interfaces_only
#import "..\csharpDll\CPUCoreUsage.tlb" raw_interfaces_only

using namespace CPUCoreUsage;
CPUUsageInterfacePtr pCPUUsage(__uuidof(Class1));

/* Define the pthread barrier */
pthread_barrier_t SyncBarrier; //Synchronization on the multiple cpu core for normal-speed communication
pthread_barrier_t SymbolBarrierHigh, SymbolBarrierLow; //High-precision timing for high-speed communication mode

/* Define glocal variables*/
long cpu_core_num = 0;
double cpu_freq = 0.0;
int package_num = 0;
int** symboldata;

int cpu_idel_flag = 0;
DWORD_PTR cpu_affinity_mask[CPU_CORE_TX_NUM] = { 0x00000001, 0x00000004, 0x00000010, 0x00000040 };
int cpu_core_id[CPU_CORE_TX_NUM] = { 0, 1, 2, 3 };



int MagneComm::ObtainCPUInfo()
{
    std::cout << "Obtain CPU Info!\n" << std::endl;

    LARGE_INTEGER litmp;
    pCPUUsage->Initialize(&cpu_init_flag);

    if (cpu_init_flag == 1) {
        pCPUUsage->GetCPUCoreNumber(&cpu_core_num);
        std::cout << "CPU Core Num is:" << cpu_core_num << std::endl;
        cpu_core_usage = (double*)malloc(cpu_core_num * sizeof(double));

        /*Generate CPU Usage pattern*/
        QueryPerformanceFrequency(&litmp);
        cpu_freq = (double)litmp.QuadPart; //CPU frequency

        return 1;
    }
    else {
        return 0;
    }
}


int MagneComm::ObtainCPUCoreUsage()
{
    std::cout << "Obatin CPU per Core Usage!\n";

    long cpuUsageState = 0;
    if (cpu_init_flag == 1) {
        pCPUUsage->GetCPUEveryCoreUseRateFloat(cpu_core_usage);
        return 1;
    }
    else {
        return 0;
    }

    //Print the CPU core usage value.
    /*std::cout << "Multicore CPU Core 0 - 3 Usage:" << cpu_core_usage[0] << " " << cpu_core_usage[1] << " " << cpu_core_usage[2] << " " << cpu_core_usage[3] << " " << std::endl;
    std::cout << "Multicore CPU Core 4 - 7 Usage:" << cpu_core_usage[4] << " " << cpu_core_usage[5] << " " << cpu_core_usage[6] << " " << cpu_core_usage[7] << " " << std::endl;
    std::cout << "Multicore CPU Core 8 -11 Usage:" << cpu_core_usage[8] << " " << cpu_core_usage[9] << " " << cpu_core_usage[10] << " " << cpu_core_usage[11] << " " << std::endl;
    std::cout << "Multicore CPU Core 12-15 Usage:" << cpu_core_usage[12] << " " << cpu_core_usage[13] << " " << cpu_core_usage[14] << " " << cpu_core_usage[15] << " " << std::endl;*/
}


void* MagneComm::OneCorePWMGenerator(void* thread_arg)
{
    LARGE_INTEGER litmp;
    LONGLONG qtime1, qtime2;

    DWORD busyTime = 0;
    DWORD idleTime = 0;

    struct thread_data_struct* thread_data;
    thread_data = (struct thread_data_struct*)thread_arg;

    DWORD_PTR affinity_flag;
    affinity_flag = SetThreadAffinityMask(GetCurrentThread(), thread_data->cpu_affinity_mask);
    //std::cout << "Previous Thread Affinity is: " << affinity_flag << std::endl;
    if (affinity_flag == 0) {
        DWORD dwErr = GetLastError();
        std::cerr << "SetThreadAffinityMask failed, GLE=" << dwErr;
    }

    pthread_barrier_wait(&SyncBarrier);

    busyTime = thread_data->time_window_length * thread_data->pulse_width_value / 100;
    idleTime = thread_data->time_window_length - busyTime;

    QueryPerformanceCounter(&litmp);
    qtime1 = litmp.QuadPart; // Start time stamp

    while (true) { // one CPU core is working 
        QueryPerformanceCounter(&litmp);
        qtime2 = litmp.QuadPart;
        if ((qtime2 - qtime1) > busyTime * cpu_freq / 1000)
            break;
    }
    Sleep(idleTime); // let cpu sleep, and the minimum length is 1ms. The accuracy is not satisfied for narrow window length
    pthread_exit(NULL);

    return NULL; //Must return a value
}


void MagneComm::PWAMGenerator(DWORD window_length, DWORD width_value, DWORD amplitude_value)
{
    pthread_t threads[CPU_CORE_TX_NUM];
    struct thread_data_struct thread_data[CPU_CORE_TX_NUM];

    for (int i = 0; i < amplitude_value; i++) {
        thread_data[i].thread_id = i;
        thread_data[i].time_window_length = window_length;
        thread_data[i].pulse_width_value = width_value; //20%
        thread_data[i].cpu_affinity_mask = cpu_affinity_mask[i];
    }

    pthread_barrier_init(&SyncBarrier, NULL, amplitude_value + 1);

    for (int i = 0; i < amplitude_value; i++) {
        pthread_create(&threads[i], NULL, OneCorePWMGenerator, (void*)&thread_data[i]);
    }

    pthread_barrier_wait(&SyncBarrier);
    pthread_barrier_destroy(&SyncBarrier);

    for (int i = 0; i < amplitude_value; i++) {
        pthread_join(threads[i], NULL);
    }
}


void MagneComm::PreambleGenerator(DWORD window_length)
{
    // preamble pattern is [31240]
    DWORD width = 100; //%
    DWORD height[CPU_CORE_TX_NUM] = { 3, 1, 2, 4 }; // 4 CPU core

    for (int i = 0; i < CPU_CORE_TX_NUM; i++) {
        PWAMGenerator(window_length, width, height[i]);
    }
    // Set height as 0 
    Sleep(window_length);
}


pwam_symbol_struct* MagneComm::ModulateData(int* package_data)
{
    struct pwam_symbol_struct* pwam_symbol_data = new pwam_symbol_struct[PACKAGE_SYMBOL_NUM];

    for (int i = 0; i < PACKAGE_SYMBOL_NUM; i++) {
        // one symbol = 4 bit, where first two bit -> PAM and last two bit -> PWM

        if (package_data[i * SYMBOL_BIT_NUM + 0] == 0 && package_data[i * SYMBOL_BIT_NUM + 1] == 0) {
            pwam_symbol_data[i].pulse_amplitude_value = 1; // 00 -> 1
        }
        if (package_data[i * SYMBOL_BIT_NUM + 0] == 0 && package_data[i * SYMBOL_BIT_NUM + 1] == 1) {
            pwam_symbol_data[i].pulse_amplitude_value = 2; // 01 -> 2
        }
        if (package_data[i * SYMBOL_BIT_NUM + 0] == 1 && package_data[i * SYMBOL_BIT_NUM + 1] == 0) {
            pwam_symbol_data[i].pulse_amplitude_value = 3; // 10 -> 3
        }
        if (package_data[i * SYMBOL_BIT_NUM + 0] == 1 && package_data[i * SYMBOL_BIT_NUM + 1] == 1) {
            pwam_symbol_data[i].pulse_amplitude_value = 4; // 11 -> 4
        }

        if (package_data[i * SYMBOL_BIT_NUM + 2] == 0 && package_data[i * SYMBOL_BIT_NUM + 3] == 0) {
            pwam_symbol_data[i].pulse_width_value = 20; // 00 -> 20%
        }
        if (package_data[i * SYMBOL_BIT_NUM + 2] == 0 && package_data[i * SYMBOL_BIT_NUM + 3] == 1) {
            pwam_symbol_data[i].pulse_width_value = 40; // 00 -> 40%
        }
        if (package_data[i * SYMBOL_BIT_NUM + 2] == 1 && package_data[i * SYMBOL_BIT_NUM + 3] == 0) {
            pwam_symbol_data[i].pulse_width_value = 60; // 00 -> 60%
        }
        if (package_data[i * SYMBOL_BIT_NUM + 2] == 1 && package_data[i * SYMBOL_BIT_NUM + 3] == 1) {
            pwam_symbol_data[i].pulse_width_value = 80; // 00 -> 80%
        }

        // Record the ideal cpu core usage
        pwam_symbol_data[i].cpu_ideal_usage = new double[cpu_core_num]();
        for (int j = 0; j < pwam_symbol_data[i].pulse_amplitude_value; j++) {
            pwam_symbol_data[i].cpu_ideal_usage[j] = (double)pwam_symbol_data[i].pulse_width_value;
        }
    }
    return pwam_symbol_data;
}


pwam_symbol_struct* MagneComm::HighSpeedModulateData2(int* package_data)
{
    struct pwam_symbol_struct* pwam_symbol_data = new pwam_symbol_struct[HIGH_SPEED_PACKAGE_SYMBOL_NUM_4BIT];

    for (int i = 0; i < HIGH_SPEED_PACKAGE_SYMBOL_NUM_4BIT; i++) {
        // one symbol = 4 bit, where first two bit -> PAM and last two bit -> PWM

        if (package_data[i * SYMBOL_BIT_NUM + 0] == 0 && package_data[i * SYMBOL_BIT_NUM + 1] == 0) {
            pwam_symbol_data[i].pulse_amplitude_value = 1; // 00 -> 1
        }
        if (package_data[i * SYMBOL_BIT_NUM + 0] == 0 && package_data[i * SYMBOL_BIT_NUM + 1] == 1) {
            pwam_symbol_data[i].pulse_amplitude_value = 2; // 01 -> 2
        }
        if (package_data[i * SYMBOL_BIT_NUM + 0] == 1 && package_data[i * SYMBOL_BIT_NUM + 1] == 0) {
            pwam_symbol_data[i].pulse_amplitude_value = 3; // 10 -> 3
        }
        if (package_data[i * SYMBOL_BIT_NUM + 0] == 1 && package_data[i * SYMBOL_BIT_NUM + 1] == 1) {
            pwam_symbol_data[i].pulse_amplitude_value = 4; // 11 -> 4
        }

        if (package_data[i * SYMBOL_BIT_NUM + 2] == 0 && package_data[i * SYMBOL_BIT_NUM + 3] == 0) {
            pwam_symbol_data[i].pulse_width_value = 20; // 00 -> 20%
        }
        if (package_data[i * SYMBOL_BIT_NUM + 2] == 0 && package_data[i * SYMBOL_BIT_NUM + 3] == 1) {
            pwam_symbol_data[i].pulse_width_value = 40; // 00 -> 40%
        }
        if (package_data[i * SYMBOL_BIT_NUM + 2] == 1 && package_data[i * SYMBOL_BIT_NUM + 3] == 0) {
            pwam_symbol_data[i].pulse_width_value = 60; // 00 -> 60%
        }
        if (package_data[i * SYMBOL_BIT_NUM + 2] == 1 && package_data[i * SYMBOL_BIT_NUM + 3] == 1) {
            pwam_symbol_data[i].pulse_width_value = 80; // 00 -> 80%
        }
    }
    return pwam_symbol_data;
}

int MagneComm::ReTransmitCheck(double* cpu_core_tx_usage, double* cpu_core_ideal_usage)
{
    int reTx_flag = 0;
    // Define the Retransmission Mechanism via CPU Usage
    for (int i = 0; i < cpu_core_num; i++) {
        if (fabs(cpu_core_tx_usage[i] - cpu_core_ideal_usage[i]) > 15.0) {
            reTx_flag = 1;
            break;
        }
    }

    return reTx_flag;
}

int MagneComm::TransmitData(char* file_path)
{
    /*Obtain the transmission data from txt file*/
    FILE* instream;
    fopen_s(&instream, file_path, "r");
    if (!instream) {
        std::cout << "Data file can't be open!\n" << file_path << std::endl;
        return -1;
    }
    else {
        char* ptr = NULL;
        int bitcount = 0;
        char txdata_s[1000];
        while (fgets(txdata_s, 1000, instream) != NULL) {
            char* pstr = strtok_s(txdata_s, "\n", &ptr);
            //std::cout << pstr[0] << std::endl;
            txfile_data[bitcount] = pstr[0] - '0';
            bitcount += 1;
        }
        fclose(instream);

        // package the data stream
        package_num = bitcount / PACKAGE_TOTAL_BIT_NUM;
        symboldata = (int**)malloc(package_num * sizeof(int*));
        for (int i = 0; i < package_num; i++) {
            symboldata[i] = (int*)malloc(PACKAGE_TOTAL_BIT_NUM * sizeof(int));
        }

        for (int i = 0; i < package_num; i++) {
            for (int j = 0; j < PACKAGE_TOTAL_BIT_NUM; j++) {
                symboldata[i][j] = txfile_data[i * PACKAGE_TOTAL_BIT_NUM + j];
            }
        }

        /*Modulate the transmission data and generate the CPU pattern*/
        struct pwam_symbol_struct* pwam_symbol_data;

        for (int i = 0; i < package_num; i++) {
            // Add the preamble on the head of the package
            std::cout << "Current package num is:" << i << std::endl;
            PreambleGenerator(2000);
            pwam_symbol_data = ModulateData(symboldata[i]);

            int retransmit_flag = 0;
            // Generate the CPU pattern for each symbol
            int j = 0;
            while (j < PACKAGE_SYMBOL_NUM) {
                PWAMGenerator(SYMBOL_TIME_LENGTH, pwam_symbol_data[j].pulse_width_value, pwam_symbol_data[j].pulse_amplitude_value);
                ObtainCPUCoreUsage();
                //std::cout << "Multicore CPU Core 0 - 3 Usage:" << cpu_core_usage[0] << " " << cpu_core_usage[1] << " " << cpu_core_usage[2] << " " << cpu_core_usage[3] << " " << std::endl;
                retransmit_flag = ReTransmitCheck(cpu_core_usage, pwam_symbol_data[j].cpu_ideal_usage);
                j++;
                //std::cout << "Retransmission Flag: " << retransmit_flag << std::endl;
                //if (retransmit_flag == 0) { // successfully transmit data
                //    j++;
                //}
                // else retransmit the symbol
            }

        }


        return 0;
    }
}


void* MagneComm::HighSpeedPWMGenerator(void* thread_arg)
{
    DWORD_PTR* thread_data;
    thread_data = (DWORD_PTR*)thread_arg;

    DWORD_PTR affinity_flag;
    affinity_flag = SetThreadAffinityMask(GetCurrentThread(), thread_data[0]);

    while (1) {
        // wait the command
        pthread_barrier_wait(&SymbolBarrierLow);
        // run the while loop, let the cpu run
        while (!cpu_idel_flag);
        // block the thread, let the cpu idel
        pthread_barrier_wait(&SymbolBarrierHigh);
    }
}


pwam_symbol_struct* MagneComm::HighSpeedModulateData(int* package_data)
{
    struct pwam_symbol_struct* pwam_symbol_data = new pwam_symbol_struct[HIGH_SPEED_PACKAGE_SYMBOL_NUM_4BIT];

    for (int i = 0; i < HIGH_SPEED_PACKAGE_SYMBOL_NUM_4BIT; i++) {
        if (package_data[i] == 0) {
            pwam_symbol_data[i].pulse_width_value = 40;
        }
        else {
            pwam_symbol_data[i].pulse_width_value = 80;
        }
    }

    return pwam_symbol_data;
}

void* MagneComm::HighSpeedTransmitControl(void* thread_arg)
{
    /*-----------------High speed communication------------------*/
        /*Modulate the transmission data and generate the CPU pattern*/
    MagneComm* magnecomm = new MagneComm();
    SetThreadAffinityMask(GetCurrentThread(), 0x00000008);
    int** thread_data;
    thread_data = (int**)thread_arg;

    pthread_t highspeed_threads[HIGH_SPEED_CPU_CORE_TX_NUM];

    pthread_barrier_init(&SymbolBarrierLow, NULL, HIGH_SPEED_CPU_CORE_TX_NUM + 1);
    pthread_barrier_init(&SymbolBarrierHigh, NULL, HIGH_SPEED_CPU_CORE_TX_NUM + 1);

    struct thread_data_struct high_speed_thread_data[HIGH_SPEED_CPU_CORE_TX_NUM];



    for (int i = 0; i < HIGH_SPEED_CPU_CORE_TX_NUM; i++) {
        // start a thread
        pthread_create(&highspeed_threads[i], NULL, HighSpeedPWMGenerator, (void*)&cpu_affinity_mask[i]);
    }


    LARGE_INTEGER litmp;
    LONGLONG time_start, time_current, time_diff;
    double cpu_freq;

    DWORD busyTime = 0, idleTime = 0, cycleTime = 0;

    /*Generate CPU Usage pattern*/
    QueryPerformanceFrequency(&litmp);
    cpu_freq = (double)litmp.QuadPart; //CPU frequency

    struct pwam_symbol_struct* pwam_symbol_data;

    cycleTime = HIGH_SPEED_SYMBOL_TIME_LENGTH * cpu_freq / 1000;

    for (int i = 0; i < package_num; i++) {
        // Add the preamble on the head of the package
        Sleep(1000);
        magnecomm->PreambleGenerator(1000);

        std::cout << "Current package num is:" << i << std::endl;
        pwam_symbol_data = HighSpeedModulateData(symboldata[i]); //thread_data

        for (int j = 0; j < HIGH_SPEED_PACKAGE_TOTAL_BIT_NUM; j++) {

            busyTime = pwam_symbol_data[j].pulse_width_value * HIGH_SPEED_SYMBOL_TIME_LENGTH / 100;
            idleTime = HIGH_SPEED_SYMBOL_TIME_LENGTH - busyTime;

            cpu_idel_flag = 0;
            pthread_barrier_wait(&SymbolBarrierLow);

            QueryPerformanceCounter(&litmp);
            time_start = litmp.QuadPart;

            while (true) { // one CPU core is working 
                QueryPerformanceCounter(&litmp);
                time_current = litmp.QuadPart;
                if ((time_current - time_start) > busyTime * cpu_freq / 1000000)
                    break;
            }

            cpu_idel_flag = 1;
            pthread_barrier_wait(&SymbolBarrierHigh);

            QueryPerformanceCounter(&litmp);
            time_start = litmp.QuadPart;

            while (true) { // one CPU core is working 
                QueryPerformanceCounter(&litmp);
                time_current = litmp.QuadPart;
                if ((time_current - time_start) > idleTime * cpu_freq / 1000000)
                    break;
            }
        }
    }

    return NULL; //Must return a value
}

int MagneComm::HighSpeedTransmitData(char* file_path)
{
    /*Obtain the transmission data from txt file*/

    LARGE_INTEGER litmp;
    LONGLONG qtime1, qtime2;
    double cpu_freq;

    DWORD busyTime = 0;
    DWORD idleTime = 0;

    /*Generate CPU Usage pattern*/
    QueryPerformanceFrequency(&litmp);
    cpu_freq = (double)litmp.QuadPart; //CPU frequency

    QueryPerformanceCounter(&litmp);
    qtime1 = litmp.QuadPart;

    FILE* instream;
    fopen_s(&instream, file_path, "r");
    if (!instream) {
        std::cout << "Data file can't be open!\n" << file_path << std::endl;
        return -1;
    }
    else {
        char* ptr = NULL;
        int bitcount = 0;
        char txdata_s[1000];
        std::cout << "Start read txt file..." << std::endl;

        while (fgets(txdata_s, 1000, instream) != NULL) {
            char* pstr = strtok_s(txdata_s, "\n", &ptr);
            //std::cout << pstr[0] << "        " << bitcount  << std::endl;
            txfile_data[bitcount] = pstr[0] - '0';
            bitcount += 1;
        }
        fclose(instream);

        // package the data stream
        package_num = bitcount / HIGH_SPEED_PACKAGE_TOTAL_BIT_NUM;
        symboldata = (int**)malloc(package_num * sizeof(int*));
        for (int i = 0; i < package_num; i++) {
            symboldata[i] = (int*)malloc(HIGH_SPEED_PACKAGE_TOTAL_BIT_NUM * sizeof(int));
        }

        for (int i = 0; i < package_num; i++) {
            for (int j = 0; j < HIGH_SPEED_PACKAGE_TOTAL_BIT_NUM; j++) {
                symboldata[i][j] = txfile_data[i * HIGH_SPEED_PACKAGE_TOTAL_BIT_NUM + j];
            }
        }

        pthread_t thread_transmit;
        pthread_create(&thread_transmit, NULL, HighSpeedTransmitControl, (void*)&symboldata);
        pthread_join(thread_transmit, NULL);

        return 0;
    }
}

