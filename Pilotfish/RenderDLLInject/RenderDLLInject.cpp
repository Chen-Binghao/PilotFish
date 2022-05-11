#include <Windows.h>
#include <cstdio>
#include <array>
#include <stdexcept>
#include <TlHelp32.h>
#include <iostream>
#include <tchar.h>
#include <comdef.h>
using namespace std;
class VirtualMemory
{
private:
	LPVOID address;
public:
	const HANDLE process;
	const SIZE_T size;
	const DWORD protectFlag;
	explicit VirtualMemory(HANDLE hProcess, SIZE_T dwSize, DWORD flProtect) :
		process(hProcess), size(dwSize), protectFlag(flProtect)
	{
		address = VirtualAllocEx(process, NULL, size, MEM_COMMIT, protectFlag);
		if (address == NULL)
			throw std::runtime_error("Failed to allocate virtual memory!");
	}
	~VirtualMemory()
	{
		if (address != NULL)
			VirtualFreeEx(process, address, 0, MEM_RELEASE);
	}
	BOOL copyFromBuffer(LPCVOID buffer, SIZE_T size)
	{
		if (size > this->size)
			return FALSE;
		return WriteProcessMemory(process, address, buffer, size, NULL);
	}
	LPVOID getAddress()
	{
		return address;
	}
};

class ChildProcess
{
private:
	STARTUPINFOA si;
	PROCESS_INFORMATION pi;
public:
	explicit ChildProcess(LPCSTR applicationPath, DWORD creationFlags)
	{
		ZeroMemory(&si, sizeof(si));
		si.cb = sizeof(si);
		ZeroMemory(&pi, sizeof(pi));
		if (!CreateProcessA(applicationPath,
			NULL, NULL, NULL, FALSE, creationFlags, NULL, NULL,
			&si, &pi))
		{
			throw std::runtime_error("Failed to create child process!");
		}
	}
	PROCESS_INFORMATION& getProcessInformation()
	{
		return pi;
	}
	~ChildProcess()
	{
		WaitForSingleObject(pi.hProcess, INFINITE);
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
	}
};

BOOL EnableDebugPriv()
{
	HANDLE   hToken;
	LUID   sedebugnameValue;
	TOKEN_PRIVILEGES   tkp;
	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
	{
		return   FALSE;
	}

	if (!LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &sedebugnameValue))
	{
		CloseHandle(hToken);
		return   FALSE;
	}
	tkp.PrivilegeCount = 1;
	tkp.Privileges[0].Luid = sedebugnameValue;
	tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

	if (!AdjustTokenPrivileges(hToken, FALSE, &tkp, sizeof(tkp), NULL, NULL))
	{
		return   FALSE;
	}
	CloseHandle(hToken);
	return TRUE;
}

FARPROC getLibraryProcAddress(LPCSTR libName, LPCSTR procName)
{
	auto dllModule = LoadLibraryA(libName);
	if (dllModule == NULL) {
		throw std::runtime_error("Unable to load library!");
	}
	auto procAddress = GetProcAddress(dllModule, procName);
	if (procAddress == NULL) {
		throw std::runtime_error("Unable to get proc address!");
	}
	return procAddress;
}

inline FARPROC getLoadLibraryAddress()
{
	return getLibraryProcAddress("kernel32.dll", "LoadLibraryA");
}

void injectWithRemoteThread(HANDLE& pi, const char* dllPath)
{
	puts("Allocating Remote Memory For dll path");
	const int bufferSize = strlen(dllPath) + 1;
	VirtualMemory dllPathMemory(pi, bufferSize, PAGE_READWRITE);
	dllPathMemory.copyFromBuffer(dllPath, bufferSize);
	PTHREAD_START_ROUTINE startRoutine = (PTHREAD_START_ROUTINE)getLoadLibraryAddress();

	puts("Creatint remote thread");
	HANDLE remoteThreadHandle = CreateRemoteThread(
		pi, NULL, NULL, startRoutine, dllPathMemory.getAddress(), CREATE_SUSPENDED, NULL);
	if (remoteThreadHandle == NULL) {
		printf("Failed to create remote thread!");
	}

	printf("Resume remote thread");
	ResumeThread(remoteThreadHandle);
	WaitForSingleObject(remoteThreadHandle, INFINITE);
	CloseHandle(remoteThreadHandle);

	//printf("Resume main thread");
	//ResumeThread(pi.hThread);
}


DWORD GetProcessIDByName(const char* szProcName)
{
	DWORD dwProcID = 0;
	HANDLE hProcessSnap;
	PROCESSENTRY32 pe32;

	hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (INVALID_HANDLE_VALUE == hProcessSnap)
		return(FALSE);

	pe32.dwSize = sizeof(PROCESSENTRY32);

	if (!Process32First(hProcessSnap, &pe32))
	{
		CloseHandle(hProcessSnap);
		cout << "!!! Failed to gather information on system processes! \n" << endl;
		return(NULL);
	}
	do
	{
		_bstr_t b(pe32.szExeFile);
		if (!strcmp(szProcName, b))
		{
			cout << b << " : " << pe32.th32ProcessID << endl;
			dwProcID = pe32.th32ProcessID;

		}
		//cout << pe32.szExeFile << endl;
	} while (Process32Next(hProcessSnap, &pe32));

	CloseHandle(hProcessSnap);

	return dwProcID;
}

int main(int argc, char* argv[])
{
	if (!EnableDebugPriv()) {
		printf("Failed to enable debug privileges");
		return -1;
	}

	//ChildProcess process("D:\\Assassins Creed Odyssey\\ACOdyssey.exe", CREATE_SUSPENDED);

	//const char* szProcessName = "AshesEscalation_DX12.exe";
	//const char* szProcessName = "Warhammer2.exe";
	//const char* szProcessName = "RDR2.exe";
	//const char* szProcessName = "HITMAN3.exe";
	//const char* szProcessName = "DIRT5.exe";
	const char* szProcessName = "SOTTR.exe";
	//const char* szProcessName = "TheDivision.exe";
	//const char* szProcessName = "F1_2021.exe";
	//const char* szProcessName = "CivilizationV_DX11.exe";
	//const char* szProcessName = "TslGame.exe";

	//Sleep(5000);
	HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, GetProcessIDByName(szProcessName));
	//HANDLE hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, 19752);
	injectWithRemoteThread(hProcess, "D:\\github\\Pilotfish\\Pilotfish\\RenderHook\\x64\\Debug\\RenderHook.dll");
	return 0;
}