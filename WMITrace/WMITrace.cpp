// WMITrace.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
// Microsoft - Windows - WMI - Activity{ 1418EF04 - B0B4 - 4623 - BF7E - D74AB47BBDAA }

extern "C" {
	typedef void(__cdecl  * NotifyCallback)(WCHAR* OutStr, bool bError);
	typedef char* (__cdecl  *GetResultCallback)(WCHAR* OutStr, DWORD GroupId, DWORD OpId);

}

class ETWProvider {

	GUID							_ProviderGuid;						// WMI-Activity
	ULONGLONG						_MatchAnyKeyword;
public:
	ETWProvider(WCHAR *GuidProvider, ULONGLONG MatchAnyKeyword);
	BOOL Enable(TRACEHANDLE &Tracehandle, ULONG ControlCode = EVENT_CONTROL_CODE_ENABLE_PROVIDER);
public:
	BOOL EnableInSession(TRACEHANDLE &Tracehandle);
	BOOL Disableinsession(TRACEHANDLE &Tracehandle);

};

class ETWWMIProvider : public ETWProvider {

public:
	ETWWMIProvider();
	static BOOL ShouldWeHandleThisEvent(USHORT Id); 
};

class ETWSession {
	BOOL								_bStarted;

	GUID								_SessionGuid;						// session Guid
	struct {
		EVENT_TRACE_PROPERTIES			Properties;
		WCHAR							SessionName[128];
		WCHAR							Buffer[1000];
	}					_LoggerInfo;

	TRACEHANDLE						_hSession;
public:
	ETWSession();
	BOOL Start();
	BOOL Stop();
	~ETWSession();
	BOOL AddProvider(ETWProvider &prov);
	WCHAR *GetName();
};

class WMIProv;
class EtwCapture;

class WMITracingSession {
	ETWSession		Session;
	ETWWMIProvider	WMiProv;
	EtwCapture *	pCapture;
	HANDLE			hThread;

public:
	void Out(NotifyCallback Notify, size_t size, bool  bError, WCHAR *Format, ...);
	WMITracingSession();
	DWORD Start(NotifyCallback Notify);
	void WMITracingSession::Stop();
	~WMITracingSession();

};


class AllocSession {
	NotifyCallback Notify;
	GetResultCallback Result;
	PVOID Context;
	BOOL Ok;
	static WMITracingSession WMIEtw;

	static std::list<AllocSession*> liste;
	// static AllocSession *FirstSession;

public:
	AllocSession(PVOID _Context, NotifyCallback _Notify, GetResultCallback _Result) : Notify(_Notify), Result(_Result), Context(_Context), Ok(FALSE){}
	DWORD Start() {
		if (Ok) return 0;
		if (liste.empty()) {
			if (WMIEtw.Start(Notify) != 0) { Ok = FALSE; return 1; }
			// FirstSession = this;
		}
		liste.push_back(this);
		Ok = TRUE;
		return 0;
	}
	static void Results(WCHAR *String, DWORD GroupId, DWORD OpId){
		for (std::list<AllocSession*>::iterator list_iter = liste.begin();
			list_iter != liste.end(); list_iter++) {

			AllocSession *p = *list_iter;
			p->Result(String,GroupId,OpId);

		}
		
		// if( FirstSession ) FirstSession->Result(String, GroupId, OpId);
	}
	void  Stop() {
		if (Ok) liste.remove(this);
		Ok = FALSE;
		if (liste.empty())
			WMIEtw.Stop();
	}
	~AllocSession(){ Stop(); }
};


class  StringArgV {
	WCHAR *Buffer;
	WCHAR *Position;
	size_t Cb;
	size_t Remaining;

public:
	void Reset() {
		Buffer[0] = 0;
		Position = Buffer;
		Remaining = Cb;
	}	
	StringArgV(size_t Size) : Buffer(new WCHAR[Size]), Cb(Size * 2) {
		Reset();
	}
	
	void Add(WCHAR *Format, WCHAR*Name, GUID &_Guid){
		LPOLESTR pwszClsid = NULL;
		if (SUCCEEDED(StringFromCLSID(_Guid, &pwszClsid))) {
			Add(Format, Name, pwszClsid);
			CoTaskMemFree(pwszClsid);
		}
	}
	void Add(WCHAR *Format, va_list &args) {
		HRESULT  hr = StringCbVPrintfEx(Position, Remaining, &Position, &Remaining, STRSAFE_NULL_ON_FAILURE, Format, args);
	}
	void Add(WCHAR *Format, ...) {
		va_list args;
		va_start(args, Format);
		HRESULT  hr = StringCbVPrintfEx(Position, Remaining, &Position, &Remaining, STRSAFE_NULL_ON_FAILURE, Format, args);

	}
	void AddTime(WCHAR *Format, WCHAR *Name, SYSTEMTIME  &st){
		WCHAR Time[15];
		StringCbPrintfW(Time, sizeof(Time), L"%2.2d:%2.2d:%2.2d.%3.3d", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
		Add(Format, Name, Time);
	}
	~StringArgV(){
		if (Buffer) delete[]Buffer;
	}
	WCHAR *GetBuf() {
		return Buffer;
	}

};


class MyWMIClientEvent {
	USHORT _Id;
	UINT32 _GroupOperationId;
	UINT32 _OperationId;
	WCHAR  *_User;
	WCHAR *_Operation;
	UINT32 _ResultCode;
	WCHAR *_ClientMachine;
	UINT32 _ClientProcessId;
	LARGE_INTEGER _Time;
	WCHAR *_Namespace;

public:
	MyWMIClientEvent(const USHORT Id, LARGE_INTEGER TimeStamp) : _Namespace(NULL) , _Time(TimeStamp), _Id(Id), _GroupOperationId(0), _OperationId(0), _User(NULL), _Operation(NULL), _ResultCode(NULL), _ClientMachine(NULL), _ClientProcessId(0) {}
	~MyWMIClientEvent() {
		if (_User != NULL)			delete[] _User;
		if (_Operation != NULL)		delete[] _Operation;
		if (_ClientMachine != NULL) delete[] _ClientMachine;
		if (_Namespace != NULL)		 delete[] _Namespace;
	}
	
	void Add(WCHAR *Property, WCHAR *Value){
		size_t i = wcslen(Value);
		
		if (_wcsicmp(Property, L"Operation") == 0) {
			_Operation = new WCHAR[ (i+1)*2];
			wcscpy_s(_Operation, (i+1)* 2, Value);
		}
		else if  (_wcsicmp(Property, L"User") == 0) {
			_User = new WCHAR[(i + 1) * 2] ;
			wcscpy_s(_User, (i + 1) * 2, Value);
		} 
		else if (_wcsicmp(Property, L"ClientMachine") == 0) {
			_ClientMachine = new WCHAR[(i + 1) * 2];
			wcscpy_s(_ClientMachine, (i + 1) * 2, Value);
		}
		else if (_wcsicmp(Property, L"Namespace") == 0) {
			_Namespace = new WCHAR[(i + 1) * 2];
			wcscpy_s(_Namespace, (i + 1) * 2, Value);
		}

		else {
			return;
		}
	

	}
	void Add(WCHAR *Property, UINT32 Value) {
		if (_wcsicmp(Property, L"GroupOperationId") == 0) {
			_GroupOperationId = Value;
		}
		else if (_wcsicmp(Property, L"OperationId") == 0) {
			_OperationId = Value;
		}
		else if (_wcsicmp(Property, L"ClientProcessId") == 0) {
			_ClientProcessId = Value;
		}
		else if (_wcsicmp(Property, L"ResultCode") == 0) {
			_ResultCode = Value;
		}
	}
	void Out() {
		StringArgV sOut(2048);
		FILETIME sttemp;
		SYSTEMTIME st;
		FileTimeToLocalFileTime((FILETIME*)&_Time, &sttemp);
		FileTimeToSystemTime(&sttemp, &st);
		sOut.AddTime(L"%s%s ", L"", st);
		if (_Id == 11 || _Id == 1 ) {
			if (_OperationId == _GroupOperationId) {
				sOut.Add(L"%s=%i ", L"Grp", _GroupOperationId);
			}
			else {
				sOut.Add(L"%s=%i ", L"Grp", _GroupOperationId);
				sOut.Add(L"%s=%i ", L"Op", _OperationId);
			}
			HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, _ClientProcessId);
			WCHAR Image[MAX_PATH];
			Image[0] = 0;
			WCHAR *p = Image;
			if (hProcess != NULL) {
				DWORD Len = GetProcessImageFileNameW(hProcess, Image, sizeof(Image));
				if (Len) {
					p += Len;
					while (*p != L'\\' && (p > Image)) { p--; };
					p++;
				}
				CloseHandle(hProcess);
			}
			sOut.Add(L"%s=%i [%s] ", L"_ClientProcessId", _ClientProcessId,p );
			if( _ClientMachine ) sOut.Add(L"%s%s ", L"", _ClientMachine);
			if( _User) sOut.Add(L"%s%s ", L"", _User);
			if( _Namespace ) sOut.Add(L"%s%s ", L"", _Namespace);
			if(  _Operation) sOut.Add(L"%s%s ", L"\n\t", _Operation);


		}
		else if (_Id == 13 || _Id == 3 ) {
			sOut.Add(L"%s ", L"Stop");
			sOut.Add(L"%s=%i ", L"Op", _OperationId);
			sOut.Add(L"%s%x ", L"0x", _ResultCode);
		}
		// printf("%S\n", sOut.GetBuf() ); // TODOTODO
		AllocSession::Results(sOut.GetBuf(), this->_GroupOperationId, this->_OperationId);
		
	}

};



ETWProvider::ETWProvider(WCHAR *GuidProvider, ULONGLONG MatchAnyKeyword) : _MatchAnyKeyword(MatchAnyKeyword){
		if (FAILED(::CLSIDFromString(GuidProvider, &_ProviderGuid))) {
			throw "CLSIDFromString  Error";
		}
}
BOOL ETWProvider::Enable(TRACEHANDLE &Tracehandle, ULONG ControlCode) {
		ENABLE_TRACE_PARAMETERS etp;
		memset(&etp, 0, sizeof(ENABLE_TRACE_PARAMETERS));
		etp.Version = ENABLE_TRACE_PARAMETERS_VERSION_2;
		etp.EnableProperty = 0;
		etp.ControlFlags = 0;
		etp.SourceId = _ProviderGuid ;
		etp.EnableFilterDesc = NULL;
		ULONG status = EnableTraceEx2(Tracehandle, (LPCGUID)&_ProviderGuid, ControlCode , TRACE_LEVEL_VERBOSE, _MatchAnyKeyword, 0, INFINITE, &etp);
		if (status != ERROR_SUCCESS) {
			return FALSE;
		}
		return TRUE;
	}

BOOL ETWProvider::EnableInSession(TRACEHANDLE &Tracehandle) { return Enable(Tracehandle); }
BOOL ETWProvider::Disableinsession(TRACEHANDLE &Tracehandle){ return Enable(Tracehandle, EVENT_CONTROL_CODE_DISABLE_PROVIDER); }
	

#define KEYWORD_WMITRACE	0x8000000000000000		//  Microsoft - Windows - WMI - Activity / Trace
#define KEYWORD_WMIOP		0x4000000000000000		// Microsoft - Windows - WMI - Activity / Operational
#define KEYWORD_WMIDEBUG	0x2000000000000000		// Microsoft - Windows - WMI - Activity / Debug



ETWWMIProvider::ETWWMIProvider() : ETWProvider(L"{1418EF04-B0B4-4623-BF7E-D74AB47BBDAA}", KEYWORD_WMITRACE) {};
BOOL ETWWMIProvider::ShouldWeHandleThisEvent(USHORT Id) {
		
		static DWORD   VersionEightOrHigher = -1 ;
		if (VersionEightOrHigher == -1) {
			if (!IsWindows8OrGreater())
				VersionEightOrHigher = 0;
			else
				VersionEightOrHigher = 1; 
		}

		if (VersionEightOrHigher) {
			if ((Id == 13 || Id == 11))
				return TRUE;
			else
				return FALSE;
		}
		if ((Id == 1 || Id == 3))
			return TRUE;
		else
			return FALSE;
}


ETWSession::ETWSession() : _hSession(NULL), _bStarted(FALSE) {
		
		ZeroMemory(&_LoggerInfo, sizeof(_LoggerInfo));
		LPOLESTR pwszClsid = NULL ;
		HRESULT hr;
		hr = ::CoCreateGuid(&_SessionGuid);

		if ( SUCCEEDED(hr) ) hr = StringFromCLSID(_SessionGuid, &pwszClsid);
		if (FAILED(hr)) {
			throw "CLSIDFromString  Error";
		}

		memset(_LoggerInfo.SessionName, 0, sizeof(_LoggerInfo.SessionName));
		StringCbPrintf(_LoggerInfo.SessionName, sizeof(_LoggerInfo.SessionName), L"%s_%s", L"WMITrace", pwszClsid);
		CoTaskMemFree(pwszClsid);

		EVENT_TRACE_PROPERTIES			&Properties = _LoggerInfo.Properties;

		Properties.Wnode.BufferSize = sizeof(_LoggerInfo); //  sizeof(EVENT_TRACE_PROPERTIES)+(wcslen(_LoggerInfo.SessionName) * 2) + 2;
	
		Properties.Wnode.Flags = /* WNODE_FLAG_USE_MOF_PTR | */  WNODE_FLAG_TRACED_GUID;							// Flags, see below
		Properties.Wnode.ClientContext = 2;																	// Time Stamp stuffs  - 1- Query performance counter, 2- System time
		Properties.Wnode.Guid = _SessionGuid;												// Guid for data block returned with results			
		Properties.BufferSize = 0;						// Lets ETW decide _BufferSize;												// buffer size for logging (kbytes)
		Properties.MinimumBuffers = 0;						// Lets ETW decide_MinimumBuffers;														// minimum to preallocate
		Properties.MaximumBuffers = 0;						// lets ETW to decide  _MaximumBuffers;														// maximum buffers allowed
		Properties.MaximumFileSize = 0;														// maximum logfile size (in MBytes)
		Properties.LogFileMode = EVENT_TRACE_REAL_TIME_MODE;								// sequential, circular
		Properties.FlushTimer = 1 ;														// buffer flush timer, in seconds
		Properties.EnableFlags = 0;														// trace enable flags => EVENT_TRACE_FLAG_DISK_IO 
		Properties.AgeLimit = 15;														// unused
		Properties.LoggerNameOffset = sizeof(_LoggerInfo.Properties);

}
BOOL ETWSession::Start() {
		if (_bStarted) return TRUE;

		ULONG status = StartTraceW(&_hSession, _LoggerInfo.SessionName , &_LoggerInfo.Properties);
		if (status != ERROR_SUCCESS) {
			return FALSE;
		}
		_bStarted = TRUE;
		return TRUE;
	}
BOOL ETWSession::Stop() {
		if (!_bStarted) return TRUE;
		ULONG status = ControlTrace(_hSession, NULL, &_LoggerInfo.Properties, EVENT_TRACE_CONTROL_STOP);
		if (status != ERROR_SUCCESS) {
			throw L"Failed to stop Trace";
		}
		_bStarted = FALSE;
		return  TRUE;
	}
ETWSession::~ETWSession() {
		if (_bStarted) Stop();
		_bStarted = FALSE;
	}
BOOL ETWSession::AddProvider(ETWProvider &prov){
		return prov.EnableInSession(_hSession);
	}
WCHAR *ETWSession::GetName() { return _LoggerInfo.SessionName; }


ULONG WINAPI   WMItraceBufferCallBack(PEVENT_RECORD pEventRecord) {

	TRACE_EVENT_INFO * pInfo = NULL;
	DWORD dwBufferSize = 0;

	

	USHORT Id = (pEventRecord->EventHeader).EventDescriptor.Id;
	
	if (!ETWWMIProvider::ShouldWeHandleThisEvent(Id)) return TRUE;

	MyWMIClientEvent MyEvent(Id, pEventRecord->EventHeader.TimeStamp);
	
	


	// Out.Add(L"%s=%s ", L"ActivityId" , pEventRecord->EventHeader.ActivityId); //   pEventRecord->EventHeader.ActivityId
	PEVENT_RECORD & Event = pEventRecord;
	// ULONG i;

	/* for ( i = 0; i < Event->ExtendedDataCount; i++) {
		switch (Event->ExtendedData[i].ExtType) {
			case EVENT_HEADER_EXT_TYPE_RELATED_ACTIVITYID:
				Out.Add(L"%s=%s" , L"RelatedActivityID", *((LPGUID)(Event->ExtendedData[i].DataPtr)));
		}
	} */

	/*
		switch (Event->ExtendedData[i].ExtType) {

		case EVENT_HEADER_EXT_TYPE_RELATED_ACTIVITYID:
			// RelatedActivityID = (LPGUID)(Event->ExtendedData[i].DataPtr);
			Out.Add(L"%S=%S ", L"RelatedActivityID", L"GUID");
			break;

		case EVENT_HEADER_EXT_TYPE_SID:
			//Sid = (PBYTE)(Event->ExtendedData[i].DataPtr);
			break;

		case EVENT_HEADER_EXT_TYPE_TS_ID:
			// SessionID = (PEVENT_EXTENDED_ITEM_TS_ID)(Event->ExtendedData[i].DataPtr);
			break;

		case EVENT_HEADER_EXT_TYPE_INSTANCE_INFO:
			//  Instance = (PEVENT_EXTENDED_ITEM_INSTANCE)(Event->ExtendedData[i].DataPtr);
			break;

		case EVENT_HEADER_EXT_TYPE_STACK_TRACE32:
			//Stack32 = (PULONG)(Event->ExtendedData[i].DataPtr);
			// FrameCount = Event->ExtendedData[i].DataSize / 4;
			break;

		case EVENT_HEADER_EXT_TYPE_STACK_TRACE64:
			// Stack64 = (PULONG64)(Event->ExtendedData[i].DataPtr);
			// FrameCount = Event->ExtendedData[i].DataSize / 8;
			break;

	

		}
	
	}
*/

	// UINT          Pid = pEventRecord->EventHeader.ProcessId;
	// UINT          Tid = pEventRecord->EventHeader.ThreadId;
	
	ULONG Status = TdhGetEventInformation(pEventRecord, 0, NULL, NULL, &dwBufferSize);
	if (ERROR_INSUFFICIENT_BUFFER == Status) {
		pInfo = (TRACE_EVENT_INFO*)malloc(dwBufferSize);
		if (pInfo == NULL)
		{
			printf("*** Error insufficient memory\n");
			return FALSE;

		}

		// Retrieve the event metadata.

		Status = TdhGetEventInformation(pEventRecord, 0, NULL, pInfo, &dwBufferSize);
		if (Status == ERROR_SUCCESS) {
			if (DecodingSourceXMLFile == pInfo->DecodingSource) {
				for (ULONG i = 0; i < pInfo->PropertyCount; i++ ) {
					EVENT_PROPERTY_INFO &EventProperty = *(pInfo->EventPropertyInfoArray + i);
					const EVENT_PROPERTY_INFO &propertyInfo = pInfo->EventPropertyInfoArray[i];
					LPCWSTR pszPropertyName = (LPCWSTR)((PBYTE)(pInfo)+propertyInfo.NameOffset);
					PROPERTY_DATA_DESCRIPTOR dataDescriptor = { (ULONGLONG)pszPropertyName, 0xFFFFFFFF, 0 };
					TDHSTATUS Status;
					PBYTE pbBuffer = NULL;
					ULONG cbBuffer = 0;
			
					
					if ((EventProperty.Flags & PROPERTY_FLAGS::PropertyStruct)== 0 ) {
						switch (EventProperty.nonStructType.InType) {
						case TDH_INTYPE_UNICODESTRING:
							
							Status = TdhGetPropertySize(pEventRecord, 0, NULL, 1, &dataDescriptor, &cbBuffer);
							pbBuffer = new BYTE[cbBuffer + 2];
							if (pbBuffer) {
								ZeroMemory(pbBuffer, cbBuffer + 2);
								Status = TdhGetProperty(pEventRecord, 0, NULL, 1, &dataDescriptor, cbBuffer, (PBYTE)pbBuffer);
								MyEvent.Add((WCHAR*)pszPropertyName, (WCHAR*)pbBuffer);
								delete[]pbBuffer;
							}

							break;
						case TDH_INTYPE_UINT32 :
						case TDH_INTYPE_HEXINT32:
							UINT32 Buffer;
							Status = TdhGetProperty(pEventRecord, 0, NULL, 1, &dataDescriptor, sizeof(UINT32), (PBYTE)&Buffer);
							MyEvent.Add((WCHAR*)pszPropertyName, Buffer);
							break;

						};

					}
					
				}
			}
		}
	}
	if (pInfo != NULL) {
		free(pInfo);
	}


	MyEvent.Out();

	// fflush(stdout);
	return TRUE;
}


class EtwCapture {
	ETWSession *			_pEtwSession;
	BOOL					_bCreated;
	TRACEHANDLE				_hConsumer;
	EVENT_TRACE_LOGFILE		_LogFile;
	PEVENT_RECORD_CALLBACK	_pCallBack;

	 friend ULONG WINAPI  WMItraceBufferCallBack(PEVENT_RECORD pEventRecord);

	 static friend DWORD WINAPI ThreadCapture(LPVOID lpParameter){
		 EtwCapture *p = (EtwCapture*)lpParameter;
		 ULONG Status = E_FAIL;
		 try {
			 Status = ProcessTrace(&p->_hConsumer, 1, NULL, NULL);
		 }
		 catch (...) {
			 printf("*** Unexpected error in CaptureThread\n");
		 }
		 return Status;
	 }


public:
	EtwCapture(ETWSession &Session, PEVENT_RECORD_CALLBACK  pCallBack) : _pCallBack(pCallBack), _pEtwSession(&Session), _bCreated(FALSE), _hConsumer((TRACEHANDLE)INVALID_HANDLE_VALUE){}
	EtwCapture(ETWProvider &Provider, PEVENT_RECORD_CALLBACK  pCallBack) : _pCallBack(pCallBack) , _pEtwSession(new ETWSession()), _bCreated(TRUE), _hConsumer((TRACEHANDLE)INVALID_HANDLE_VALUE) {
		_pEtwSession->Start();
		_pEtwSession->AddProvider(Provider);
	}
	HANDLE  StartRealTimeCapture( ) {
		if (_pEtwSession == NULL) throw L"No ETWSession";
		EVENT_TRACE_LOGFILE LogFile;
		ZeroMemory(&LogFile, sizeof(EVENT_TRACE_LOGFILE));
		LogFile.LogFileMode = PROCESS_TRACE_MODE_EVENT_RECORD | EVENT_TRACE_REAL_TIME_MODE;
		LogFile.LoggerName = _pEtwSession->GetName();
		LogFile.Context = 0;
		LogFile.EventRecordCallback = (PEVENT_RECORD_CALLBACK)_pCallBack;


		_hConsumer = OpenTraceW (&LogFile);
		if (_hConsumer == (TRACEHANDLE)INVALID_HANDLE_VALUE) {
			return NULL;
		}
		return CreateThread(NULL, 0, ThreadCapture, this , NULL, NULL);
	}
	void StopRealTimeCapture() {
		if ((TRACEHANDLE)INVALID_HANDLE_VALUE != _hConsumer) {
			CloseTrace(_hConsumer);
			_hConsumer = (TRACEHANDLE)INVALID_HANDLE_VALUE;
		}
	}
	~EtwCapture() {
		StopRealTimeCapture();
		if (_bCreated) {
			delete _pEtwSession; _pEtwSession = NULL;
		}
	}
};



void WMITracingSession::Out(NotifyCallback Notify, size_t size, bool  bError, WCHAR *Format, ...) {
		StringArgV s(size);
		va_list args;
		va_start(args, Format);
		// s.Add(L"%s", L"DEBUGNOtify- ");
		s.Add(Format, args);
		Notify(s.GetBuf(), (bool)bError);
}

WMITracingSession::WMITracingSession() : hThread(NULL), pCapture(NULL) {}
DWORD WMITracingSession::Start(NotifyCallback Notify) {

		if (!Session.Start()) {
			Out(Notify,100, TRUE, L"Failed to start session %d\n", GetLastError());;
			return GetLastError();
		}
		StringArgV Msg(100);

		Out(Notify,100, FALSE, L"*** Successfully Created ETW Session %s\n", Session.GetName());

		if (!Session.AddProvider(WMiProv)) {
			Out(Notify, 100, TRUE, L"Failed to add VMI Provider in ETW Session %d\n", GetLastError());
			return GetLastError();
		}
		Out(Notify,100, FALSE, L"*** Successfully Added Provider to  ETW Session\n");

		pCapture = new EtwCapture(Session, (PEVENT_RECORD_CALLBACK)WMItraceBufferCallBack);
		hThread = pCapture->StartRealTimeCapture();
		if (hThread != NULL) return GetLastError();
		return ERROR_SUCCESS;
}
void WMITracingSession::Stop() {
		Session.Stop();
		if (hThread) {
			WaitForSingleObject(hThread, 1000);
			CloseHandle(hThread);
			hThread = NULL;
		}
		if (pCapture) {
			delete pCapture;
			pCapture = NULL;
		}
}

WMITracingSession::~WMITracingSession() { Stop(); }

 WMITracingSession AllocSession::WMIEtw;
 std::list<AllocSession*> AllocSession::liste;
 // AllocSession* AllocSession::FirstSession;


 extern "C" __declspec(dllexport)  PVOID Start(PVOID Context, NotifyCallback Notify, GetResultCallback Result) {
	 AllocSession *p = NULL;
	try {
		p = new AllocSession(Context,Notify,Result);
		DWORD status = p->Start();
		if (status == 0) return p;
		return NULL;
	}
	catch (...) {
		if (p) delete p; 
		return NULL;
	}
}

extern "C"  __declspec(dllexport)   void Stop(VOID *ar) {
	AllocSession *p = (AllocSession*)ar;
	p->Stop();

	delete p;
}

 extern "C" __declspec(dllexport)  void StartAndWait(PVOID Handle, PVOID Context, NotifyCallback Notify, GetResultCallback Result) {
	 
	PVOID Status  = Start(Context, Notify, Result);
	if (Status != 0) {
		WaitForSingleObject(Handle, INFINITE);
		Stop(Status);
		return ;
	}
	return ;
 }


/*
int _tmain(int argc, _TCHAR* argv[])
{
	::CoInitialize(NULL);
	SetConsoleCtrlHandler(ConsoleHandler, TRUE);

	VOID *HandleSession = Start(0);
	if (HandleSession == NULL) return 1;
	hEvent = CreateEventW(NULL, FALSE, FALSE, NULL);
	WaitForSingleObject(hEvent, 1000000);

	Stop(HandleSession);
	return 0;
}

*/

/*int _tmain(int argc, _TCHAR* argv[])
{
	::CoInitialize(NULL);
	SetConsoleCtrlHandler(ConsoleHandler, TRUE);
	
	ETWSession Session;
	ETWWMIProvider WMiProv;
	DWORD Status ;
	hEvent = CreateEventW(NULL, FALSE, FALSE, NULL);


	try {
	
		if (!Session.Start()) {
			printf("Failed to start session %d\n", GetLastError());
			return GetLastError();
		}
		printf("*** Successfully Created ETW Session %S\n", Session.GetName());

		if (!Session.AddProvider(WMiProv)) {
			printf("Failed to add VMI Provider in ETW Session %d\n", GetLastError());
			return GetLastError();
		}
		printf("*** Successfully Added Provider to  ETW Session\n");

		EtwCapture Capture(Session, (PEVENT_RECORD_CALLBACK)WMItraceBufferCallBack);
		HANDLE hThread = Capture.StartRealTimeCapture();
		HANDLE ah[] = { hThread, hEvent };
		Status = WaitForMultipleObjects(2, ah, FALSE, 2000000);
		Session.Stop();
		if (Status == (WAIT_OBJECT_0 + 1) ) {
			printf("*** Control Event Detected... Stopping ...\n");
		}
		WaitForSingleObject(hThread, 1000);
		CloseHandle(hThread);
	}
	catch (...) {
		printf("*** Unexpected\n");
	}
	printf("*** Ending\n");
	CloseHandle(hEvent);
	
	return 0;
}
*/
