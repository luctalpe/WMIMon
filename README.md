
## WMIMon

This command line tool allows to monitor WMI activity on Windows platform.

# Features

It is a real-time ETL consumer for the WMI-Activity event log channel.
It will allow to also get information about the WMI client process (executable).
You can specify a regular expression to filter and limit output to a specific executable,username,client computername, Process ID,query. 

# Scenarios

This tool may be useful for several scenarios:
- Finding which executable/computer/user are executing specific queries and putting load on your system
- Learn WMI queries done by your components or a component tha you need to troubleshoot
- Execute a specific script when a WMI error code is returned to a client

# Sample 1

- Allow to view all WMI activity
```
C:\Temp>WMIMOn
***** *** Successfully Created ETW Session WMITrace_{1B701051-0E73-4EEE-85B7-567AC21B1E55}

***** *** Successfully Added Provider to  ETW Session

***** 14:38:22.372 Grp=125426 _ClientProcessId=3092 [MsMpEng.exe] LUCT10 NT AUTHORITY\SYSTEM
        IWbemServices::Connect
***** 14:38:22.376 Grp=125427 Op=125428 _ClientProcessId=3092 [MsMpEng.exe] LUCT10 NT AUTHORITY\SYSTEM
        Start IWbemServices::CreateInstanceEnum - root\SecurityCenter2 : AntiVirusProduct
***** 14:38:22.380 Stop Op=125426 0x0
***** 14:38:22.380 Stop Op=125428 0x0

```
# Sample 2

-Will monitor all queries containing CreateSnaphost. When this query is executed, the prowershell script listvar.ps1 is executed.This script will display all WMIMON powershell variable and will display informations for the WMI current process ($WMIMOM_PID variable)

```
PS C:\temp\WMIMon> type .\listvar.ps1
ls variable:WMI*
get-process -ID $WMIMON_PID


PS C:\temp\WMIMon> .\WMIMon.exe "-filter=.*Virtual.*CreateSnapshot" "-action=.\listvar.ps1"
Parsing:        filtering on .*virtual.*createsnapshot
Parsing:        Powershell action when filter is found : .\listvar.ps1
***** *** Successfully Created ETW Session WMITrace_{81830E71-72D7-4228-94CE-A02FE99A01B8}

***** *** Successfully Added Provider to  ETW Session

***** 14:46:46.615 Grp=12388022 Op=12388023 _ClientProcessId=3448 [mmc.exe] LUCT2016 LUCT2016\luct
        Start IWbemServices::ExecMethod - root\virtualization\v2 : \\.\ROOT\virtualization\v2:Msvm_VirtualSystemSnapshot
Service.CreationClassName="Msvm_VirtualSystemSnapshotService",Name="vssnapsvc",SystemCreationClassName="Msvm_ComputerSys
tem",SystemName="LUCT2016"::CreateSnapshot

Name                           Value
----                           -----
WMIMON_PID                     3448
WMIMON_EXECUTABLE              mmc.exe
WMIMON_COMPUTER                LUCT2016
WMIMON_USER                    LUCT2016\luct
WMIMON_STOPSTATUS              0
WMIMON_ACTIVITY                14:46:46.615 Grp=12388022 Op=12388023 _ClientProcessId=3448 [mmc.exe] LUCT2016 LUCT201...
WMIMON_RELATEDACTIVITY

Id      : 3448
Handles : 1715
CPU     : 17070.078125
SI      : 2
Name    : mmc



***** 14:46:46.659 Stop Op=12388023 0x0

```

# Usage

- WMItrace.exe is a basic C++ version without any filtering capability
- WMIMON.exe is a .Net tool with all the features. You need to copy WMIMonC.dll in the same directory

```
c:\Temp>WMImon /?
Parsing:        Invalid argument /?


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

```

