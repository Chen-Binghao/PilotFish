using namespace std;

ofstream hook_log;
ofstream debug_log;
ofstream kernel_log;
ofstream kernel_unhooked_log;
ofstream kernel_unregistered_log;
ofstream kernel_avetime;
int kernel_count = 0;
int getfunction_count = 0;
double timeslice = 10;
bool time_update_flag = true;
bool busy = true;
char time_update = '0';
int kernel_slice_count = 0;
SRWLOCK time_lock;
SRWLOCK busy_lock;
map<CUfunction, string> kernel_function;
map<CUfunction, int> kernel_num;
map<CUfunction, int> kernel_unhooked;
map<CUfunction, string> kernel_unregistered;
map<CUfunction, float> kernel_totaltime;
map<CUfunction, int> kernel_app;
map<CUfunction, double> kernel_time;
map<string, double> kernel_offline;

HANDLE flipMap;
HANDLE timeMap;
HANDLE busyMap;
HANDLE kernelMap;
LPVOID flipBuffer = NULL;// 共享内存指针  
LPVOID timeBuffer = NULL;
LPVOID busyBuffer = NULL;
LPVOID kernelBuffer = NULL;// 共享内存指针  
string kernelData = "0";//用于存储共享内存中的数据
#define BUF_SIZE 256
char flipB[BUF_SIZE] = { 0 };
char timeB[256] = { 0 };
char busyB[BUF_SIZE] = { 0 };

//CUresult CUDAAPI cuLaunchKernel
typedef CUresult(CUDAAPI* cuLaunchKernelt)(CUfunction f, unsigned int gridDimX, unsigned int gridDimY, unsigned int gridDimZ, unsigned int blockDimX, unsigned int blockDimY, unsigned int blockDimZ, unsigned int sharedMemBytes, CUstream hStream, void** kernelParams, void** extra);
cuLaunchKernelt ocuLaunchKernel = NULL;
cuLaunchKernelt cuLaunchKerneladdr;

typedef CUresult(CUDAAPI* cuModuleGetFunctiont)(CUfunction* hfunc, CUmodule hmod, const char* name);
cuModuleGetFunctiont ocuModuleGetFunction = NULL;
cuModuleGetFunctiont cuModuleGetFunctionaddr;

DWORD WINAPI flip_detection(LPVOID lpParam);

bool get_time_status()
{
	//read the status
	strcpy(flipB, (char*)flipBuffer);
	if (flipB[0] == '0')
		return false;
	if (flipB[0] == '1')
		return true;
	return false;
}

double read_timeslice()
{
	strcpy(timeB, (char*)timeBuffer);
	return atof(string(timeB).c_str());
}

double get_timeslice()
{
	if (time_update_flag != get_time_status())
	{
		timeslice = read_timeslice();
		time_update_flag = get_time_status();
		//debuglog << kernel_slice_count << endl;
		//kernel_slice_count = 0;
	}
	return timeslice;
}

bool is_gpu_busy()
{
	strcpy(busyB, (char*)busyBuffer);
	if (busyB[0] == '0')
	{
		busy = false;
		return false;
	}
	if (busyB[0] == '1')
	{
		busy = true;
		return true;
	}
	//kernel_appear << "busy error!!!!" << endl;
	return false;
}

void get_kernel_time(map<string, double>& kernel_list)
{
	ifstream inFile("E:\\CloudGaming\\OfflineData\\kernel_avetime.txt");
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

bool launch_available(double cur_time)
{
	AcquireSRWLockShared(&time_lock);
	bool flag = (busy || (timeslice < cur_time));
	ReleaseSRWLockShared(&time_lock);
	return flag;
}

DWORD WINAPI flip_detection(LPVOID lpParam)
{
	while (true)
	{
		/*strcpy(flipB, (char*)flipBuffer);*/
		AcquireSRWLockExclusive(&busy_lock);
		is_gpu_busy();
		ReleaseSRWLockExclusive(&busy_lock);
		if (time_update_flag != get_time_status())
		{
			AcquireSRWLockExclusive(&time_lock);
			is_gpu_busy();
			timeslice = read_timeslice();
			time_update_flag = get_time_status();
			//LARGE_INTEGER stamp;
			//QueryPerformanceCounter(&stamp);
			//debuglog << "kernelnum " << kernel_slice_count << " time slice " << timeslice << " flip " << time_update_flag << "busy" << busy << "    " << stamp.QuadPart / 10000 << endl;
			//kernel_slice_count = 0;
			ReleaseSRWLockExclusive(&time_lock);
			//write kernelbuffer
			kernelData = to_string(kernel_count);
			strcpy((char*)kernelBuffer, kernelData.c_str());//写入数据
			kernel_count = 0;
		}
		Sleep(1);
	}
}

CUresult CUDAAPI hkcuLaunchKernel(CUfunction f, unsigned int gridDimX, unsigned int gridDimY, unsigned int gridDimZ, unsigned int blockDimX, unsigned int blockDimY, unsigned int blockDimZ, unsigned int sharedMemBytes, CUstream hStream, void** kernelParams, void** extra)
{
	/*double cur_time = 0.1;
	if (kernel_time.find(f) != kernel_time.end())
	{
		cur_time = kernel_time.find(f)->second;
	}
	else
	{
		presentlog << "can't find!!!! " << kernel_function.find(f)->second <<  endl;
	}*/
	double cur_time = kernel_time.find(f)->second;

	//while (is_gpu_busy())
	//while (busy || (timeslice < cur_time))
	while (launch_available(cur_time))
	{
		Sleep(1);
	}
	//if (kernel_slice_count == 0) debuglog << "timeslice :  " << timeslice << "  time:  " << cur_time << endl;
	AcquireSRWLockExclusive(&time_lock);
	timeslice -= cur_time;
	ReleaseSRWLockExclusive(&time_lock);
	//kernel_slice_count++;
	kernel_count++;

	CUresult ret = ocuLaunchKernel(f, gridDimX, gridDimY, gridDimZ, blockDimX, blockDimY, blockDimZ, sharedMemBytes, hStream, kernelParams, extra);

	return ret;
}
//load offline data
CUresult CUDAAPI hkcuModuleGetFunction(CUfunction* hfunc, CUmodule hmod, const char* name)
{
	string str(name);
	kernel_function.insert(pair<CUfunction, string>(*hfunc, str));
	kernel_num.insert(pair<CUfunction, int>(*hfunc, 0));
	if (kernel_offline.find(str) != kernel_offline.end())
	{
		kernel_time.insert(pair<CUfunction, double>(*hfunc, kernel_offline.find(str)->second));
	}

	CUresult ret = ocuModuleGetFunction(hfunc, hmod, name);
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
		InitializeSRWLock(&time_lock);
		InitializeSRWLock(&busy_lock);
		flipMap = OpenFileMapping(FILE_MAP_ALL_ACCESS, 0, L"flip");// 先判断要打开的共享内存名称是否存在
		timeMap = OpenFileMapping(FILE_MAP_ALL_ACCESS, 0, L"time");
		busyMap = OpenFileMapping(FILE_MAP_ALL_ACCESS, 0, L"busy");
		kernelMap = OpenFileMapping(FILE_MAP_ALL_ACCESS, 0, L"kernel");
		flipBuffer = MapViewOfFile(flipMap, FILE_MAP_ALL_ACCESS, 0, 0, 0);
		timeBuffer = MapViewOfFile(timeMap, FILE_MAP_ALL_ACCESS, 0, 0, 0);
		busyBuffer = MapViewOfFile(busyMap, FILE_MAP_ALL_ACCESS, 0, 0, 0);
		kernelBuffer = MapViewOfFile(kernelMap, FILE_MAP_ALL_ACCESS, 0, 0, 0);

		hook_log.open("E:\\CloudGaming\\Log\\hook_log.txt");
		debug_log.open("E:\\CloudGaming\\Log\\debug_log.txt");
		kernel_log.open("E:\\CloudGaming\\Log\\kernel_log.txt");
		kernel_unhooked_log.open("E:\\CloudGaming\\Log\\kernel_unhooked_log.txt");
		kernel_unregistered_log.open("E:\\CloudGaming\\Log\\kernel_unregistered_log.txt");
		kernel_avetime.open("E:\\CloudGaming\\Log\\kernel_avetime.txt");

		get_kernel_time(kernel_offline);
		CreateThread(NULL, NULL, flip_detection, NULL, 0, NULL);

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
		if (MH_CreateHook((LPVOID)cuLaunchKerneladdr, hkcuLaunchKernel, (LPVOID*)&ocuLaunchKernel) != MH_OK) hook_log << "create cuLaunchKernel failed" << endl;
		else hook_log << "create cuLaunchKernel success" << endl;
		if (MH_CreateHook((LPVOID)cuModuleGetFunctionaddr, hkcuModuleGetFunction, (LPVOID*)&ocuModuleGetFunction) != MH_OK) hook_log << "create cuModuleGetFunction failed" << endl;
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

		if (MH_DisableHook((LPVOID)&cuLaunchKerneladdr) != MH_OK) { return 1; }
		if (MH_DisableHook((LPVOID)&cuModuleGetFunctionaddr) != MH_OK) { return 1; }
		if (MH_Uninitialize() != MH_OK) { return 1; }
		break;
	}

	return TRUE;
}
