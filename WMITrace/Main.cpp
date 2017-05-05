
#include "stdafx.h"

HANDLE	hEvent = NULL;

BOOL  WINAPI  ConsoleHandler(
	DWORD dwCtrlType   //  control signal type
	) {
	SetEvent(hEvent);
	return TRUE;
}

extern "C" {
	typedef void(__cdecl  * NotifyCallback)(WCHAR* OutStr, bool bError);
	typedef void (__cdecl  *GetResultCallback)(WCHAR* OutStr, DWORD GroupId, DWORD OperationId);

}

extern "C" LPVOID Start(PVOID Context, NotifyCallback Notify, GetResultCallback Result);
extern "C" void Stop(VOID *ar);

static TCHAR**Argv;
static int Count = 0;

BOOL SearchI(WCHAR *s1, WCHAR *s2) {

	size_t l1 = wcslen(s1) + 1;
	WCHAR *il1 = new WCHAR[l1];
	size_t l2 = wcslen(s2) + 1;
	WCHAR *il2 = new WCHAR[l2];

	for (size_t i = 0; i < l1; i++) 	il1[i] = towupper(s1[i]);
	for (size_t j = 0; j < l2 ; j++) 	il2[j] = towupper(s2[j]);

	BOOL bresult;
	bresult = wcsstr(il1, il2) == NULL ? FALSE : TRUE;
	delete[] il1;
	delete[] il2;
	return bresult;
}

extern "C" void __cdecl NotifyP(WCHAR* OutStr, bool bError) {
	printf("%s%S", (bError ? "ERROR : " : ""), OutStr);
}

HANDLE hExternalStopEvent = NULL;
BOOL bStopRequested = FALSE;

extern "C" void __cdecl ResultP(WCHAR*OutStr,DWORD GroupId,DWORD OperationId) {
	bool bOutput = TRUE;
	// printf("Count %d\n", Count );
	if (Count > 0) {
		bOutput = FALSE;
		for (int i = 0; i < Count; i++) {
			if (StrStrI(OutStr, Argv[i]) != NULL) {
				bOutput = TRUE;
				break;
			}
		}
	}
	if (bOutput) {
		printf("%S\n", OutStr);
		fflush(stdout);
		if (bStopRequested) {
			printf("Pattern found ... Stopping ... \n");
			::SetEvent(hExternalStopEvent);
			return;
		}
	}
}

int _tmain(int argc, _TCHAR* argv[])
{
	::CoInitialize(NULL);
	SetConsoleCtrlHandler(ConsoleHandler, TRUE);



	WCHAR *Cmd = GetCommandLineW();
	if (SearchI(Cmd, L"/stop") || SearchI(Cmd, L"-stop")) {
		bStopRequested = TRUE;
	}
	Argv = argv+1;
	Count = argc - 1;
	
	hEvent = CreateEventW(NULL, FALSE, FALSE, NULL);
	if( hEvent ) hExternalStopEvent = CreateEventW(NULL, FALSE, FALSE, L"WMITRACE_STOP");
	if (hEvent == NULL || hExternalStopEvent == NULL) {
		DWORD err = GetLastError();
		printf("Unexpected error in CreateEvent %d", err);
		return err;
	}	

	VOID *HandleSession = Start(0,NotifyP,ResultP);

	if (HandleSession == NULL) return 1;

	HANDLE h[] = { hEvent, hExternalStopEvent };
	WaitForMultipleObjects(2, h, FALSE, 3600 * 1000);

	Stop(HandleSession);
	return 0;
}
