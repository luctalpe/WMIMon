using System;
using System.Collections.Generic;
using System.Text;
using System.Threading;
using System.Runtime.InteropServices;
using System.Text.RegularExpressions;


using System.Management.Automation;
using System.Management.Automation.Runspaces;

using System.Collections.ObjectModel;

// Main
namespace WMIMon
{
   
    
    class Program
    {
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        delegate void NotifyCallback([MarshalAs(UnmanagedType.LPWStr)] string OutStr, bool bError);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        delegate void GetResultCallBack([MarshalAs(UnmanagedType.LPWStr)] string OutStr, UInt32 GroupId, UInt32 OpId);

        [DllImport("WmiMonC.dll")]
        static extern IntPtr Start(IntPtr Context , [MarshalAs(UnmanagedType.FunctionPtr)]NotifyCallback Notify , [MarshalAs(UnmanagedType.FunctionPtr)]GetResultCallBack Result) ;

        [DllImport("WmiMonC.dll")]
        static extern void StartAndWait(IntPtr Handle, IntPtr Context, [MarshalAs(UnmanagedType.FunctionPtr)]NotifyCallback Notify, [MarshalAs(UnmanagedType.FunctionPtr)]GetResultCallBack Result);

        [DllImport("WmiMonC.dll")]
        static extern void Stop(IntPtr Context);

        private static bool IsValidRegex(string pattern)
        {
            if (string.IsNullOrEmpty(pattern)) return false;

            try
            {
                Regex.Match("", pattern);
            }
            catch (ArgumentException)
            {
                return false;
            }

            return true;
        }
        public enum StopCondition { none = 0, start = 1, stop = 2 };

        // .\WMIMon.exe "[-filter=regularexpression]" "[-stop=[start|end|none]]"   [-log=all|filter] [-action=powershellpipeline]]
        static void Main(string[] args)
        {

            string filter = "";
           
            StopCondition  uStop =  StopCondition.none ;
            bool LogOnFilter = true;
            UInt64 IfStopStatus = 0;
            bool bAction = false;
            string PipeLine = "";
            Runspace runSpace = RunspaceFactory.CreateRunspace();
            runSpace.Open();

            Dictionary<UInt64, string> PendingOp = new Dictionary<UInt64, string>();

    

            foreach (string arg in args)
            {
                Match m;
                
                m = Regex.Match(arg, "-(f|fi|fil|filt|filt|filte|filter)=(.+)", RegexOptions.IgnoreCase);
                if (m.Success)
                {
                    filter = m.Groups[2].ToString().ToLower();
                    Console.WriteLine("Parsing:\tfiltering on {0}", filter);
                    if (!IsValidRegex(filter))
                    {
                        Console.WriteLine("Parsing:\tInvalid regular expression {0}", filter);
                        return;
                    }
                    continue;
                }

                m = Regex.Match(arg, "-(s|st|sto|stop)=(start|end|none)", RegexOptions.IgnoreCase);
                if (m.Success)
                {
                    string stop = m.Groups[2].ToString().ToLower();
                    uStop = StopCondition.none;
                    if (stop == "start")  uStop = StopCondition.start;
                    else if ( stop == "end" ) uStop = StopCondition.stop;
                    Console.WriteLine("Parsing:\twill stop if filter matches {0}", stop);
                    continue;
                }
                m = Regex.Match(arg, "-(l|lo|log)=(all|filter)", RegexOptions.IgnoreCase);
                if (m.Success)
                {
                    string log = m.Groups[2].ToString().ToLower();
                    Console.WriteLine("Parsing:\tlogging option : {0} ", log);
                    if (log == "all") LogOnFilter = false; else LogOnFilter = true;

 
                    continue;
                }
                m = Regex.Match(arg, "-(i|if|ifs|ifsto|ifstop|ifstops|ifstopst|ifstopstat|ifstopstatu|ifstopstatus)=(0x[0-9,a-f]+|[0-9,a-f]+)", RegexOptions.IgnoreCase);
                if (m.Success)
                {
                    // bAction = true;
                    IfStopStatus = Convert.ToUInt64(m.Groups[2].ToString(),16);
                    Console.WriteLine("Parsing:\tPowershell will end if status is : 0x{0:x} ", IfStopStatus);
                    continue;
                }

                m = Regex.Match(arg, "-(a|ac|act|acti|actio|action)=(.+)", RegexOptions.IgnoreCase);
                if (m.Success)
                {
                    bAction = true;
                    PipeLine = m.Groups[2].ToString();
                    Console.WriteLine("Parsing:\tPowershell action when filter is found : {0} ", PipeLine);
                }
                else 
                {
                    Console.WriteLine("Parsing:\tInvalid argument {0}\n", arg);
                    Console.WriteLine(
 @"
Usage:  WmiMon [-filter=regular_expression_string] [-stop=start|end|none] [-ifstopstatus=hexadecimal_value] [-log=all|filter] [action=pipeline]
                  default WmiMon [-filter=.*] [-stop=none] [-log=all]

will monitor WMI activity. By default all WMI activities are displayed. 

You can filter the output with the -filter switch.

You can stop the application :
- if the filtering is successfull. Stop will occur at activity startup  if -stop=start is specified.
      If -stop=end is specified we will wait for the end of the activity to stop the monitoring
        Warning : if many records match the filtering pattern , memory usage  may increase  
- if the filtering is successfull and _ifstopstatus condition is meet
    Warning : if many records match the filtering pattern , memory usage for this query may be hudge  

For all filtered items or if a stop condition is meet , the pipeline action will be executed         
Powershell variables WMIMON_* will be set in Powershell runspace to reflect the current WMI activity.
Your Powershell actions may use these variables (client PID, client computer, client user, stop status, WMI query,...)  

N.B: WMIMon is based on RealTime ETL notification. ETL infrastructure doesn't guarantee that all events will be received.
N.B: WMI Stop operation logging may occur after a delay based on client (get-cim* cmdlets cleanup occurs immediately 
     This is not true with get-wmiobject cmdlet).

Feel Free to report any bug or suggestion to luct@microsoft.com

Example: 
"

);

                    return;
                }

            }

            var exitEvent = new ManualResetEvent(false);
            Console.CancelKeyPress += (sender, eventArgs) =>
            {
                eventArgs.Cancel = true;
                exitEvent.Set();
            };


            NotifyCallback Notify = (string OutStr, bool bError) =>
            {
                if (bError)
                {
                    Console.WriteLine("******! Error  {0}", OutStr);
                }
                else
                    Console.WriteLine("***** {0}", OutStr);
            };
            GetResultCallBack Result = (string OutStr, UInt32 GroupId, UInt32 OpId) =>
            {
                bool bDisplay = true;
                bool bStopOperation = false;
                UInt64 StopStatus = 0;
                string ClientProcessId = "";
                string Executable = "";
                string Computer = "";
                string User = "";
                bool bFilter = (filter.Length != 0) ? true : false; 
                bool bFilterMatch = false;
                bool bStop = false;
             
                string PendingQuery = "";
                bool bDisplayStop = false; 

                Match m;
                m = Regex.Match(OutStr, "^\\d\\d:\\d\\d:\\d\\d.\\d\\d\\d Stop Op=\\d+ 0x([0-9a-fA-F]+)", RegexOptions.IgnoreCase);
                if (m.Success) {
                    bStopOperation = true;
                    bDisplayStop = true;
                    StopStatus = Convert.ToUInt64(m.Groups[1].ToString(), 16);
                    

                    if( bFilter ) {
                        bDisplayStop = false; 
                        
                        //is this operation in the Pending list
                        if( PendingOp.TryGetValue(OpId, out PendingQuery) )
                        {
                            m = Regex.Match(PendingQuery, "^\\d\\d:\\d\\d:\\d\\d.\\d\\d\\d .* _ClientProcessId=(\\d+) \\[(.*)\\] (.*) (.*) ", RegexOptions.IgnoreCase);
                            if (m.Success)
                            {
                                ClientProcessId = m.Groups[1].ToString();
                                Executable = m.Groups[2].ToString();
                                Computer = m.Groups[3].ToString();
                                User = m.Groups[4].ToString();
                                bDisplayStop = true;
                            }
                            PendingOp.Remove(OpId);
                            if( (IfStopStatus != 0) && (StopStatus == IfStopStatus) )
                            {
                                bStop = true;
                            } else if (StopCondition.stop == uStop )
                            {
                                bStop = true;
                            }
                            // Console.WriteLine("==== Debug : Removing Pending Stop {0} \\ {1}\\ bStop {2} ", OutStr, PendingQuery , bStop );
                        }

                    }

                }
                else
                {
                    bStopOperation = false;
                    m = Regex.Match(OutStr, "^\\d\\d:\\d\\d:\\d\\d.\\d\\d\\d .* _ClientProcessId=(\\d+) \\[(.*)\\] (.*) (.*) ", RegexOptions.IgnoreCase);
                    if( m.Success)
                    {
                        ClientProcessId = m.Groups[1].ToString();
                        Executable = m.Groups[2].ToString();
                        Computer = m.Groups[3].ToString();
                        User = m.Groups[4].ToString();
                    }
                    
                }
                if (!bStopOperation)
                {
                    if (bFilter)
                    {
                        string outwithoutn = OutStr.Replace("\n", "");
                        MatchCollection  mFilter = Regex.Matches(outwithoutn, filter, RegexOptions.IgnoreCase | RegexOptions.Multiline);
                        if (mFilter.Count > 0 ) bFilterMatch = true;
                       
                    }
                }
                // at this point
                // bFilter ==> if filter
                // bFilterMatch ==> if filter match 
                // bLogFilter ==> true -log=filter
                //            ==> false -log=all
                // uStop ==> StopCondition.none , StopCondition.start , StopCondition.end
                // bAction == TRUE ==> pipeline
                bDisplay = false;
              
                if( bFilter )
                {
                    bDisplay = false;
                    if( bFilterMatch && LogOnFilter == true )
                    {
                        bDisplay = true;
                    }
                    else if( LogOnFilter == false  )
                    {
                        bDisplay = true;
                    }
                    if( uStop == StopCondition.start  && bFilterMatch )
                    {
                        bStop = true;
                    }
                    else if ( uStop == StopCondition.stop && bFilterMatch )
                    {
                        // TODO : add to stoppending list
                    }
                    if( bFilter && bFilterMatch )
                    {
                        PendingOp.Add(OpId, OutStr);
                       // Console.WriteLine("==== Debug Adding {0} in Pending list ", OpId);
                    }
                }
                else
                {
                    bDisplay = true;
                   
                }
                if (bDisplay || bDisplayStop)
                {
                   Console.WriteLine("***** {0}", OutStr);
                }

                if ( (bAction && bFilter && bFilterMatch ) | (bStop && bFilter ) ) 
                {
                    // TODO Execute Pipeline 
                    runSpace.SessionStateProxy.SetVariable("WMIMON_PID", ClientProcessId);
                    runSpace.SessionStateProxy.SetVariable("WMIMON_EXECUTABLE", Executable);
                    runSpace.SessionStateProxy.SetVariable("WMIMON_COMPUTER", Computer);
                    runSpace.SessionStateProxy.SetVariable("WMIMON_USER", User);
                    runSpace.SessionStateProxy.SetVariable("WMIMON_STOPSTATUS", StopStatus);
                    runSpace.SessionStateProxy.SetVariable("WMIMON_ACTIVITY", OutStr);
                    runSpace.SessionStateProxy.SetVariable("WMIMON_RELATEDACTIVITY", PendingQuery);
                    Pipeline pipeline = runSpace.CreatePipeline();
                    String script = PipeLine + " | out-string ";
                    pipeline.Commands.AddScript(script);

                    Collection<PSObject> Results; 
                    try
                    {
                        Results = pipeline.Invoke();
                        foreach (PSObject PsObj  in Results) {
                            Console.WriteLine(PsObj.ToString());
                        }
                    }
                    catch (PSInvalidOperationException ioe)
                    {
                        Console.WriteLine ("Powershell Error: " + ioe.Message);
                        pipeline.Stop();
                        pipeline = null;
               
                    }
                    catch(System.Management.Automation.RuntimeException error)
                    {
                        ErrorRecord e = error.ErrorRecord;
                        Console.WriteLine("Powershell Error: {0}{1} ", e.ToString(), e.InvocationInfo.PositionMessage);
                        pipeline.Stop();
                        pipeline = null;
                    }


                }
                
                if (bStop)
                {
                    exitEvent.Set();
                }
               
                    
            };

            IntPtr Context = (IntPtr)1;
            IntPtr Handle = exitEvent.SafeWaitHandle.DangerousGetHandle();
            try
            {
                StartAndWait((IntPtr)Handle, Context, Notify, Result); // cf https://msdn.microsoft.com/en-us/library/7esfatk4(VS.71).aspx

            }
            catch (SystemException e)
            {
                Console.WriteLine("Unexpected error {0} ", e);
            }
        }

    }
}
