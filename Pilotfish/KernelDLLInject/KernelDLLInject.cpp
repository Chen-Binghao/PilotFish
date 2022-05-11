#include <iostream>
#include <Windows.h>
#include <cstdio>
#include <array>
#include <stdexcept>
#include <string>

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
			throw runtime_error("Failed to allocate virtual memory!");
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
	explicit ChildProcess(LPSTR command, LPCSTR dir, DWORD creationFlags)
	{
		ZeroMemory(&si, sizeof(si));
		si.cb = sizeof(si);
		ZeroMemory(&pi, sizeof(pi));
		if (!CreateProcessA(NULL,
			command, NULL, NULL, FALSE, creationFlags, NULL, dir,
			&si, &pi))
		{
			throw runtime_error("Failed to create child process!");
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
		throw runtime_error("Unable to load library!");
	}
	auto procAddress = GetProcAddress(dllModule, procName);
	if (procAddress == NULL) {
		throw runtime_error("Unable to get proc address!");
	}
	return procAddress;
}

inline FARPROC getLoadLibraryAddress()
{
	return getLibraryProcAddress("kernel32.dll", "LoadLibraryA");
}

void injectWithRemoteThread(PROCESS_INFORMATION& pi, const char* dllPath)
{
	cout << "Allocating Remote Memory For dll path" << endl;
	const int bufferSize = strlen(dllPath) + 1;
	VirtualMemory dllPathMemory(pi.hProcess, bufferSize, PAGE_READWRITE);
	dllPathMemory.copyFromBuffer(dllPath, bufferSize);
	PTHREAD_START_ROUTINE startRoutine = (PTHREAD_START_ROUTINE)getLoadLibraryAddress();

	cout << "Creating remote thread" << endl;
	HANDLE remoteThreadHandle = CreateRemoteThread(
		pi.hProcess, NULL, NULL, startRoutine, dllPathMemory.getAddress(), CREATE_SUSPENDED, NULL);
	if (remoteThreadHandle == NULL) {
		cout << "Failed to create remote thread!" << endl;
	}

	cout << "Resume remote thread" << endl;
	ResumeThread(remoteThreadHandle);
	WaitForSingleObject(remoteThreadHandle, INFINITE);
	CloseHandle(remoteThreadHandle);

	cout << "Resume main thread" << endl;
	ResumeThread(pi.hThread);
}


int main(int argc, char* argv[])
{
	//Sleep(10000);
	//return 0;
	if (!EnableDebugPriv()) {
		cout << "Failed to enable debug privileges" << endl;
		return -1;
	}

	char cmd[1024];
	sprintf_s(cmd, "%s%s%s", argv[1], " ", argv[2]);
	//cout << cmd << endl;
	if (argc != 5)
	{
		cout << "参数数量不正确" << endl;
		MessageBoxA(0, "参数数量不正确", "error", 1);
		return 1;
	}
	ChildProcess process(cmd, argv[3], CREATE_SUSPENDED);
	//ChildProcess process(command, dir, CREATE_SUSPENDED);
	injectWithRemoteThread(process.getProcessInformation(), argv[4]);
	//injectWithRemoteThread(process.getProcessInformation(), "D:\\github\\Pilotfish\\Pilotfish\\KernelHook\\x64\\Debug\\KernelHook.dll");
	//injectWithRemoteThread(process.getProcessInformation(), "E:\\CloudGaming\\Pilotfish单机\\KernelHook\\x64\\Debug\\KernelHook.dll");
	//injectWithRemoteThread(process.getProcessInformation(), "E:\\CloudGaming\\Pilotfish单机\\exe\\KernelHook_profile_full.dll");
	//injectWithRemoteThread(process.getProcessInformation(), "E:\\CloudGaming\\Pilotfish单机\\exe\\KernelHook_profile.dll");
	//injectWithRemoteThread(process.getProcessInformation(), "E:\\CloudGaming\\Pilotfish单机\\exe\\KernelHook_co.dll");
	//injectWithRemoteThread(process.getProcessInformation(), "E:\\CloudGaming\\Pilotfish单机\\exe\\KernelHook_naive.dll");
	//injectWithRemoteThread(process.getProcessInformation(), "E:\\CloudGaming\\Pilotfish单机\\exe\\KernelHook_kc.dll");
	//injectWithRemoteThread(process.getProcessInformation(), "E:\\CloudGaming\\Pilotfish单机\\exe\\KernelHook_to.dll");
	//injectWithRemoteThread(process.getProcessInformation(), "D:\\github\\Pilotfish\\Pilotfish单机\\KernelHook\\x64\\Debug\\KernelHook.dll");

	return 0;
}