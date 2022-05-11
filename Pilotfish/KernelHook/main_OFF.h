#include <thread>
#include <map>
#include <iostream>
#include <process.h>

using namespace std;

ofstream hook_log;
ofstream debug_log;
ofstream kernel_log;
ofstream kernel_unhooked_log;
ofstream kernel_unregistered_log;
ofstream kernel_avetime;
int kernel_count = 0;
map<CUfunction, string> kernel_function;
map<CUfunction, int> kernel_num;
map<CUfunction, int> kernel_unhooked;
map<CUfunction, string> kernel_unregistered;
map<CUfunction, float> kernel_totaltime;
map<CUfunction, int> kernel_app;
map<CUfunction, double> kernel_time;
map<string, double> kernel_offline;

//CUresult CUDAAPI cuLaunchKernel
typedef CUresult(CUDAAPI* cuLaunchKernelt)(CUfunction f, unsigned int gridDimX, unsigned int gridDimY, unsigned int gridDimZ, unsigned int blockDimX, unsigned int blockDimY, unsigned int blockDimZ, unsigned int sharedMemBytes, CUstream hStream, void** kernelParams, void** extra);
cuLaunchKernelt ocuLaunchKernel = NULL;
cuLaunchKernelt cuLaunchKerneladdr;

typedef CUresult(CUDAAPI* cuModuleGetFunctiont)(CUfunction* hfunc, CUmodule hmod, const char* name);
cuModuleGetFunctiont ocuModuleGetFunction = NULL;
cuModuleGetFunctiont cuModuleGetFunctionaddr;

void get_kernel_time(map<string, double>& kernel_list)
{
	ifstream inFile(KERNELAVG1);
	string time;
	string name;
	while (getline(inFile, name))
	{
		getline(inFile, time);
		kernel_list.insert(pair<string, double>(name, atof(time.c_str())));
	}
	inFile.close();
	return;
}

//offline
CUresult CUDAAPI hkcuLaunchKernel(CUfunction f, unsigned int gridDimX, unsigned int gridDimY, unsigned int gridDimZ, unsigned int blockDimX, unsigned int blockDimY, unsigned int blockDimZ, unsigned int sharedMemBytes, CUstream hStream, void** kernelParams, void** extra)
{
	if (kernel_num.find(f) != kernel_num.end())
	{
		kernel_num.find(f)->second += 1;
	}
	else
	{
		if (kernel_unhooked.find(f) != kernel_unhooked.end())
		{
			kernel_unhooked.find(f)->second += 1;
		}
		else
		{
			kernel_unhooked.insert(pair<CUfunction, int>(f, 1));
		}
	}

	/*if (kernel_time.find(f) == kernel_time.end())
	{
		hook_log << "can't find !!!!:   " << f << '\n' << kernel_function.find(f)->second << endl;
		if (kernel_unregistered.find(f) == kernel_unregistered.end())
			kernel_unregistered.insert(pair<CUfunction, string>(f, kernel_function.find(f)->second));
	}*/

	cudaEvent_t start, stop;
	float elapsedTime;

	cudaEventCreate(&start);
	cudaEventRecord(start, 0);

	//Do kernel activity here
	CUresult ret = ocuLaunchKernel(f, gridDimX, gridDimY, gridDimZ, blockDimX, blockDimY, blockDimZ, sharedMemBytes, hStream, kernelParams, extra);

	cudaEventCreate(&stop);
	cudaEventRecord(stop, 0);
	cudaEventSynchronize(stop);

	cudaEventElapsedTime(&elapsedTime, start, stop);

	if (kernel_totaltime.find(f) != kernel_totaltime.end())
	{
		kernel_totaltime.find(f)->second += elapsedTime;
	}
	else
	{
		kernel_totaltime.insert(pair<CUfunction, float>(f, elapsedTime));
	}
	return ret;
}

//load offline data
CUresult CUDAAPI hkcuModuleGetFunction(CUfunction* hfunc, CUmodule hmod, const char* name)
{
	string str(name);
	//kernel_function.insert(pair<CUfunction, string>(*hfunc, str));
	//kernel_num.insert(pair<CUfunction, int>(*hfunc, 0));
	/*if (kernel_offline.find(str) != kernel_offline.end())
	{
		kernel_time.insert(pair<CUfunction, double>(*hfunc, kernel_offline.find(str)->second));
	}*/

	CUresult ret = ocuModuleGetFunction(hfunc, hmod, name);
	kernel_function.insert(pair<CUfunction, string>(*hfunc, str));
	kernel_num.insert(pair<CUfunction, int>(*hfunc, 0));
	return ret;
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


//=========================================================================================================================//

BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD fdwReason, LPVOID)
{
	DisableThreadLibraryCalls(hInstance);

	switch (fdwReason)
	{
	case DLL_PROCESS_ATTACH:
		timeBeginPeriod(1);

		hook_log.open("D:\\CloudGaming\\Log\\hook_log.txt");
		debug_log.open("D:\\CloudGaming\\Log\\debug_log.txt");
		kernel_log.open("D:\\CloudGaming\\Log\\kernel_log.txt");
		kernel_unhooked_log.open("D:\\CloudGaming\\Log\\kernel_unhooked_log.txt");
		kernel_unregistered_log.open("D:\\CloudGaming\\Log\\kernel_unregistered_log.txt");
		kernel_avetime.open(KERNELAVG2);

		//get_kernel_time(kernel_offline);

		if (hook_log)MessageBoxA(0, "DLL injected", "step 4", 3);
		hook_log << "dll inject" << endl;
		DisableThreadLibraryCalls(hInstance);
		GetModuleFileNameA(hInstance, dlldir, 512);
		for (size_t i = strlen(dlldir); i > 0; i--) { if (dlldir[i] == '\\') { dlldir[i + 1] = 0; break; } }
		cuLaunchKerneladdr = (cuLaunchKernelt)getLibraryProcAddress("nvcuda.dll", "cuLaunchKernel");
		cuModuleGetFunctionaddr = (cuModuleGetFunctiont)getLibraryProcAddress("nvcuda.dll", "cuModuleGetFunction");
		hook_log << "culaucnkernel addr " << cuLaunchKerneladdr << endl;
		hook_log << "cuModuleGetFunction addr" << cuModuleGetFunctionaddr << endl;

		if (MH_Initialize() != MH_OK) hook_log << "initialize hook failed" << endl; else hook_log << "initialize hook sucess" << endl;
		if (MH_CreateHook((LPVOID)cuLaunchKerneladdr, hkcuLaunchKernel, (LPVOID*)& ocuLaunchKernel) != MH_OK) hook_log << "create cuLaunchKernel failed" << endl;
		else hook_log << "create cuLaunchKernel success" << endl;
		if (MH_CreateHook((LPVOID)cuModuleGetFunctionaddr, hkcuModuleGetFunction, (LPVOID*)& ocuModuleGetFunction) != MH_OK) hook_log << "create cuModuleGetFunction failed" << endl;
		else hook_log << "create cuModuleGetFunction success" << endl;

		if (MH_EnableHook((LPVOID)cuLaunchKerneladdr) != MH_OK) hook_log << "enable cuLaunchKernel failed" << endl;
		else hook_log << "enable cuLaunchKernel success" << endl;
		if (MH_EnableHook((LPVOID)cuModuleGetFunctionaddr) != MH_OK) hook_log << "enable cuModuleGetFunction failed" << endl;
		else hook_log << "enable cuModuleGetFunction success" << endl;

		break;
	case DLL_PROCESS_DETACH: // A process unloads the DLL.
		timeEndPeriod(1);

		//kernel num
		map<CUfunction, int>::iterator i;
		i = kernel_num.begin();
		map<CUfunction, string> ::iterator j;
		while (i != kernel_num.end())
		{
			if (i->second > 0 && kernel_function.find(i->first) != kernel_function.end())
			{
				kernel_log << kernel_function.find(i->first)->second << '\n' << i->second << endl;
			}
			i++;
		}

		//kernel time
		map<CUfunction, float>::iterator k;
		k = kernel_totaltime.begin();
		while (k != kernel_totaltime.end())
		{
			if (kernel_function.find(k->first) != kernel_function.end() && kernel_num.find(k->first) != kernel_num.end())
			{
				kernel_avetime << kernel_function.find(k->first)->second << '\n' << (k->second) / (kernel_num.find(k->first)->second) << endl;
			}
			if (kernel_unhooked.find(k->first) != kernel_unhooked.end())
			{
				kernel_unhooked_log << k->first << '\n' << (k->second) / (kernel_unhooked.find(k->first)->second) << '\n' << kernel_unhooked.find(k->first)->second << endl;
			}
			k++;
		}

		MessageBoxA(0, "DLL removed", "step 4", 3);

		if (MH_DisableHook((LPVOID)& cuLaunchKerneladdr) != MH_OK) { return 1; }
		if (MH_DisableHook((LPVOID)& cuModuleGetFunctionaddr) != MH_OK) { return 1; }
		if (MH_Uninitialize() != MH_OK) { return 1; }
		break;
	}

	return TRUE;
}
