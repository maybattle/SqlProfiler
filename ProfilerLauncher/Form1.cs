/*****************************************************************************
 * ProfilerTest
 * 
 * Copyright (c) 2006 Scott Hackett
 * 
 * This software is provided 'as-is', without any express or implied warranty. 
 * In no event will the author be held liable for any damages arising from the
 * use of this software.
 * 
 * Permission to use, copy, modify, distribute and sell this software for any 
 * purpose is hereby granted without fee, provided that the above copyright 
 * notice appear in all copies and that both that copyright notice and this 
 * permission notice appear in supporting documentation.
 * 
 * Scott Hackett (code@scotthackett.com)
 *****************************************************************************/

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;
using System.Windows.Forms;
using System.Diagnostics;
using System.IO;

namespace ProfilerLauncher
{


    public partial class Form1 : Form
    {
        [UnmanagedFunctionPointer(CallingConvention.StdCall)]
        public delegate void SqlDataCallback(int valueFromcallback);

        [DllImport("DotNetProfiler.dll")]
        public static extern void SetSqlCallback([MarshalAs(UnmanagedType.FunctionPtr)] SqlDataCallback pointer);

        //readonly SqlDataCallback callback = (command) =>SetText(command);
       TcpReceiveServer _tcpReceiver = new TcpReceiveServer();
        

       

        private const string PROFILER_GUID = "{9E2B38F2-7355-4C61-A54F-434B7AC266C0}";
                                                                          

        public Form1()
        {
            this.Closing += (s, e) => _tcpReceiver.Close();
            InitializeComponent();
            _tcpReceiver.Open();
        }


        private void runEf_Click(object sender, EventArgs e)
        {
            RunAndProfileProcess("HelloWorld.exe");
        }

        private void runLinq_Click(object sender, EventArgs e)
        {
           
            RunAndProfileProcess("LinqToSql.exe");
        }


        private static void RunAndProfileProcess(string pathToExecutable)
        {
            ProcessStartInfo psi;

            // make sure the executable exists
            if (File.Exists(pathToExecutable) == false)
            {
                MessageBox.Show("The executable '" + pathToExecutable +
                                "' does not exist.\nCheck the EXECUTABLE_TO_RUN constant in the program.");
                return;
            }

            // create a process executor
            psi = new ProcessStartInfo(pathToExecutable);

            // ----- SET THE CLR ENVIRONMENT VARIABLES -------------------

            // set the Cor_Enable_Profiling env var. This indicates to the CLR whether or
            // not we are using the profiler at all.  1 = yes, 0 = no.
            if (psi.EnvironmentVariables.ContainsKey("COR_ENABLE_PROFILING") == true)
                psi.EnvironmentVariables["COR_ENABLE_PROFILING"] = "1";
            else
                psi.EnvironmentVariables.Add("COR_ENABLE_PROFILING", "1");

            // set the COR_PROFILER env var. This indicates to the CLR which COM object to
            // instantiate for profiling.
            if (psi.EnvironmentVariables.ContainsKey("COR_PROFILER") == true)
                psi.EnvironmentVariables["COR_PROFILER"] = PROFILER_GUID;
            else
                psi.EnvironmentVariables.Add("COR_PROFILER", PROFILER_GUID);

            // ----- SET THE PROFILER ENVIRONMENT VARIABLES -------------------

            // set the LOG_FILENAME env var. This indicates to our profiler where the log 
            // file should be written to.
            if (psi.EnvironmentVariables.ContainsKey("LOG_FILENAME") == true)
                psi.EnvironmentVariables["LOG_FILENAME"] = @"ICorProfilerCallback Log.log";
            else
                psi.EnvironmentVariables.Add("LOG_FILENAME", @"ICorProfilerCallback Log.log");

            // ----- RUN THE PROCESS -------------------

            psi.UseShellExecute = false;
            Process p = Process.Start(psi);
        }

       

     

    }
}