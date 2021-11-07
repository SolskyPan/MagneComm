using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Diagnostics;

namespace CPUCoreUsage//ClassLibrary3
{
    public interface CPUUsageInterface
    {
        int Add(int Number1, int Number2);
        int Times(int Number1, int Number2);
        int Initialize();
        int GetCPUCoreNumber();
        string GetCPUEveryCoreUseRateString();
        unsafe void GetCPUEveryCoreUseRateFloat(double* resultRate);

    }

    public class Class1 : CPUUsageInterface
    {
        private int m_nCPUCoreNumber;
        private PerformanceCounter[] m_pfCounters;

        public int Add(int Number1, int Number2)
        {
            return Number1 + Number2;
        }

        public int Times(int Number1, int Number2)
        {
            return Number1 * Number2;
        }

        public int Initialize()
        {
            try
            {
                m_nCPUCoreNumber = System.Environment.ProcessorCount;     //Get the num of CPU Cores
                m_pfCounters = new PerformanceCounter[m_nCPUCoreNumber];  //Get each core's usage
                for (int i = 0; i < m_nCPUCoreNumber; i++)
                {
                    m_pfCounters[i] = new PerformanceCounter("Processor", "% Processor Time", i.ToString());
                }
            }
            catch (System.Exception e)
            {
                return 0;
            }
            return 1;//If successfully initialize, return 1.
        }

        public int GetCPUCoreNumber()
        {
            return m_nCPUCoreNumber;
        }

        public string GetCPUEveryCoreUseRateString()
        {
            StringBuilder strBuild = new StringBuilder();
            float fRate = m_pfCounters[0].NextValue();
            int nRate = Convert.ToInt32(fRate);
            strBuild.Append(nRate.ToString());
            for (int i = 1; i < m_nCPUCoreNumber; i++)
            {
                fRate = m_pfCounters[i].NextValue();  //Get each core's usage
                nRate = Convert.ToInt32(fRate);   //Convert to int       
                strBuild.Append("," + nRate.ToString()); //Add as string
            }
            return strBuild.ToString();
        }

        public unsafe void GetCPUEveryCoreUseRateFloat(double* resultRate)
        {
            //float * resultRate = new float[m_nCPUCoreNumber];
            double fRate = 0.0;

            for (int i = 0; i < m_nCPUCoreNumber; i++)
            {
                fRate = m_pfCounters[i].NextValue();  //Get each core's usage
                resultRate[i] = fRate;
            }
        }
    }
}