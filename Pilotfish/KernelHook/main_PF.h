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
int getfunction_count = 0;
double timeslice = 10;
bool time_update_flag = true;
bool busy = true;
char time_update = '0';
int kernel_slice_count = 0;
int frame_s = 0;
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
LPVOID flipBuffer = NULL;//   
LPVOID timeBuffer = NULL;
LPVOID busyBuffer = NULL;

string kernellaunchnumData = "0";
LPVOID kernellaunchnumBuffer = NULL;
HANDLE kernellaunchnumMap;
int kernellaunchnum = 0;
string kernellaunchtimeData = "0";
LPVOID kernellaunchtimeBuffer = NULL;
HANDLE kernellaunchtimeMap;
float kernellaunchtime = 0;
string kernelremaintimeData = "0";
LPVOID kernelremaintimeBuffer = NULL;
HANDLE kernelremaintimeMap;
float kernelremaintime = 0;
string kernelremainnumData = "0";
LPVOID kernelremainnumBuffer = NULL;
HANDLE kernelremainnumMap;
int kernelremainnum = 0;
string mempoolData = "1";//
LPVOID mempoolBuffer = NULL;//                                                   
HANDLE mempoolMap;
#define BUF_SIZE 256
#define POOL_SIZE 100000
#define MAP_SIZE 4096
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
cudaEvent_t eventpool[POOL_SIZE];


typedef CUresult(CUDAAPI* cuMemAlloct)(CUdeviceptr* dptr, size_t bytesize);
cuMemAlloct ocuMemAlloc = NULL;
cuMemAlloct cuMemAllocaddr;

typedef CUresult(CUDAAPI* cuMemFreet)(CUdeviceptr dptr);
cuMemFreet ocuMemFree = NULL;
cuMemFreet cuMemFreeaddr;

typedef CUresult(CUDAAPI* cuCtxCreatet)(CUcontext* pctx, unsigned int  flags, CUdevice dev);
cuCtxCreatet ocuCtxCreate = NULL;
cuCtxCreatet cuCtxCreateaddr;
int CopyDtoH();
int CopyHtoD();

HANDLE* m_hEvent;
HANDLE startEvent1, startEvent2;
//typedef cudaError_t(CUDARTAPI* cudaMalloct)(void** devPtr, size_t size);
//cudaMalloct ocudaMalloc = NULL;
//cudaMalloct cudaMallocaddr;
CUdeviceptr* test_cumemptr_addr;
CUdeviceptr test_cumemptr;
size_t test_cumems;
int launchnum = 0;
bool valid_addr = true;
bool model_init = false;
bool train_prepare = true;
int train_status = 0;
CUcontext cu_pctx;
CUdevice cu_device;
CUstream stream = 0;
map<CUdeviceptr, size_t> vmem_map;
map<CUdeviceptr, void*> mem_vmem_map;
map<CUdeviceptr, CUdeviceptr*> table_vmem_map;
map<CUdeviceptr, float> memcpy_time;
map<CUdeviceptr, int> event_map;
CUevent _Tstart_events;
vector<int> pause_num;
vector<int> resume_num;
int event_num = 0;
struct Queryparam
{
	cudaEvent_t event1;
	cudaEvent_t event2;
	float time;
};
queue<Queryparam*> kernelrecordqueue;
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
		train_status = 1;
		cudaDeviceReset();
		busy = true;
		return true;
	}
	//kernel_appear << "busy error!!!!" << endl;
	return false;
}

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

void setkernellaunchnum(int num)
{
	kernellaunchnumData = to_string(num);
	strcpy((char*)kernellaunchnumBuffer, kernellaunchnumData.c_str());//写入数据
	return;
}

void setkernellaunchtime(float time)
{
	kernellaunchtimeData = to_string(time);
	strcpy((char*)kernellaunchtimeBuffer, kernellaunchtimeData.c_str());//写入数据
	return;
}

void setkernelremainnum(int num)
{
	kernelremainnumData = to_string(num);
	strcpy((char*)kernelremainnumBuffer, kernelremainnumData.c_str());//写入数据
	return;
}

void setkernelremaintime(float time)
{
	kernelremaintimeData = to_string(time);
	strcpy((char*)kernelremaintimeBuffer, kernelremaintimeData.c_str());//写入数据
	return;
}

bool launch_available(double cur_time)
{
	AcquireSRWLockShared(&time_lock);
	AcquireSRWLockShared(&busy_lock);
	bool flag = (timeslice < cur_time);
	ReleaseSRWLockShared(&busy_lock);
	ReleaseSRWLockShared(&time_lock);
	return flag;
}

DWORD WINAPI flip_detection(LPVOID lpParam)
{
	float duration = 0;
	cudaError_t ret;
	while (true)
	{
		/*strcpy(flipB, (char*)flipBuffer);*/
		AcquireSRWLockExclusive(&busy_lock);
		is_gpu_busy();
		ReleaseSRWLockExclusive(&busy_lock);
		if (time_update_flag != get_time_status())
		{
			if (train_status == 1)
			{
				CopyHtoD();
				train_status = 0;
			}
			frame_s = (frame_s + 1) % 240;
			if (frame_s == 0)
			{
				CopyDtoH();
			}
			AcquireSRWLockExclusive(&time_lock);
			//is_gpu_busy();
			timeslice = read_timeslice();
			time_update_flag = get_time_status();
			//LARGE_INTEGER stamp;
			//QueryPerformanceCounter(&stamp);
			//debuglog << "kernelnum " << kernel_slice_count << " time slice " << timeslice << " flip " << time_update_flag << "busy" << busy << "    " << stamp.QuadPart / 10000 << endl;
			//kernel_slice_count = 0;
			ReleaseSRWLockExclusive(&time_lock);
			cudaError_t ret2;
			while (!kernelrecordqueue.empty())
			{
				Queryparam* a = kernelrecordqueue.front();
				ret2 = cudaEventQuery(a->event2);
				if (ret2 == cudaSuccess)
				{
					kernellaunchnum++;
					kernellaunchtime += a->time;
					kernelremainnum--;
					//cudaEventElapsedTime(&duration, a->event1, a->event2);
					kernelremaintime -= a->time;
					//kernelremaintime += duration;
					debug_log << kernellaunchnum << "," << kernellaunchtime << "," << kernelremainnum << "," << kernelremaintime << endl;
					//cudaEventDestroy(eventpool[a->event1]);
					cudaEventDestroy(a->event2);
					//delete a;
					kernelrecordqueue.pop();
				}
				else
				{
					break;
				}
			}
			timeslice -= kernelremaintime;
			setkernellaunchnum(kernellaunchnum);
			setkernellaunchtime(kernellaunchtime);
			setkernelremainnum(kernelremainnum);
			setkernelremaintime(kernelremaintime);
		}
		Sleep(1);
	}
}

int init_mempool()
{
	LARGE_INTEGER HookBeginStamp;
	CUevent copyEnd;
	CUresult cuStatus;
	map<CUdeviceptr, size_t>::iterator iter;
	int i = 0;

	cuStatus = cuEventCreate(&copyEnd, CU_EVENT_BLOCKING_SYNC);
	if (cuStatus != CUDA_SUCCESS)
	{
		debug_log << "[init test4] cuEventCreate failed 4 " << " cuStatus " << cuStatus << endl;
	}
	int total_size = 0;
	CUdeviceptr cumemptr;
	size_t cumems;

	for (iter = vmem_map.begin(); iter != vmem_map.end(); iter++)
	{
		//分配页锁定内存
		//Sleep(5000);
		//a = (void*)malloc(test_cumems);
		void* a;
		cumemptr = iter->first;
		cumems = iter->second;
		total_size += cumems;
		QueryPerformanceCounter(&HookBeginStamp);
		//debuglog << "[pause] DtoH iter " << i << " size " << test_cumems << " B    at " << HookBeginStamp.QuadPart << endl;
		cuStatus = cuMemAllocHost((void**)&a, cumems);
		if (cuStatus != CUDA_SUCCESS)
		{
			debug_log << "[init test4] cuMemHostAlloc failed " << HookBeginStamp.QuadPart << " cuStatus " << cuStatus << endl;
			return 1;
		}

		//a = (void*)malloc(test_cumems);

		//Sleep(2000);
		debug_log << "addr " << cumemptr << " size " << cumems << endl;

		mem_vmem_map.insert(pair<CUdeviceptr, void*>(cumemptr, a));
		i++;
	}

	mempoolMap = ::CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0,
		total_size,
		L"mempool");

	cuEventRecord(copyEnd, stream);
	cuEventSynchronize(copyEnd);
	debug_log << "[init] init mempool sucess total size " << total_size << " B   iter " << vmem_map.size() << endl;

	cuEventDestroy(copyEnd);
	return 0;
}

int CopyDtoH()
{
	LARGE_INTEGER HookBeginStamp;
	QueryPerformanceCounter(&HookBeginStamp);
	debug_log << "[pause] begin, total " << vmem_map.size() << " iters " << endl;
	CUevent* eventStart;
	CUevent copyStart;
	CUevent* eventEnd;
	CUevent copyEnd;
	CUstream stream = 0;
	CUresult cuStatus;
	map<CUdeviceptr, size_t>::iterator iter;
	float ms, total_ms, total_ms2;
	int i = 0;
	eventStart = new CUevent[vmem_map.size()];
	eventEnd = new CUevent[vmem_map.size()];

	for (int i = 0; i < vmem_map.size(); i++)
	{
		cuStatus = cuEventCreate(&eventStart[i], CU_EVENT_BLOCKING_SYNC);
		if (cuStatus != CUDA_SUCCESS)
		{
			debug_log << "[test4] cuEventCreate failed 1 " << HookBeginStamp.QuadPart << " cuStatus " << cuStatus << endl;
		}
		cuStatus = cuEventCreate(&eventEnd[i], CU_EVENT_BLOCKING_SYNC);
		if (cuStatus != CUDA_SUCCESS)
		{
			debug_log << "[test4] cuEventCreate failed 2 " << HookBeginStamp.QuadPart << " cuStatus " << cuStatus << endl;
		}
	}
	cuStatus = cuEventCreate(&copyStart, CU_EVENT_BLOCKING_SYNC);
	if (cuStatus != CUDA_SUCCESS)
	{
		debug_log << "[test4] cuEventCreate failed 3 " << HookBeginStamp.QuadPart << " cuStatus " << cuStatus << endl;
	}
	cuStatus = cuEventCreate(&copyEnd, CU_EVENT_BLOCKING_SYNC);
	if (cuStatus != CUDA_SUCCESS)
	{
		debug_log << "[test4] cuEventCreate failed 4 " << HookBeginStamp.QuadPart << " cuStatus " << cuStatus << endl;
	}
	int total_size = 0;
	total_ms = 0;
	cuEventRecord(copyStart, stream);
	cuEventSynchronize(copyStart);//finish all previous jobs
	cuEventRecord(copyStart, stream);

	CUdeviceptr cumemptr;
	size_t cumems;
	for (iter = vmem_map.begin(); iter != vmem_map.end(); iter++)
	{
		cumemptr = iter->first;
		cumems = iter->second;
		if (mem_vmem_map.find(cumemptr) == mem_vmem_map.end())
		{
			debug_log << "[pause test4] a not found " << i << " ptr " << cumemptr << endl;
		}
		void* a = mem_vmem_map.find(cumemptr)->second;
		total_size += cumems;
		QueryPerformanceCounter(&HookBeginStamp);
		//debuglog << "[pause] DtoH iter " << i << " size " << test_cumems << " B    at " << HookBeginStamp.QuadPart << endl;

		//copy
		cuEventRecord(eventStart[i], stream);
		cuStatus = cuMemcpyDtoH(a, cumemptr, cumems);
		cuEventRecord(eventEnd[i], stream);
		if (cuStatus != CUDA_SUCCESS)
		{
			debug_log << "[test4] cuMemcpyDtoH failed " << HookBeginStamp.QuadPart << " cuStatus " << cuStatus << endl;
		}
		i++;
	}
	cuEventRecord(copyEnd, stream);
	cuEventSynchronize(copyEnd);
	i = 0;
	for (iter = vmem_map.begin(); iter != vmem_map.end(); iter++)
	{
		cuEventElapsedTime(&ms, eventStart[i], eventEnd[i]);
		total_ms += ms;
		QueryPerformanceCounter(&HookBeginStamp);
		//debuglog << "[pause] DtoH iter " << i << "    size " << iter->second << " B    takes " << ms << " ms   at " << HookBeginStamp.QuadPart << endl;
		i++;
	}
	cuEventElapsedTime(&ms, copyStart, copyEnd);
	debug_log << "[pause] save totally takes " << total_ms << " ms    actually: " << ms << " ms     size " << total_size << " B" << endl;
	if (vmem_map.size() != mem_vmem_map.size()) debug_log << "[pause] two map not the same length!" << endl;
	i = 0;
	int addr_s = 0;
	for (iter = vmem_map.begin(); iter != vmem_map.end(); iter++)
	{
		cumemptr = iter->first;
		cumems = iter->second;
		if (mem_vmem_map.find(cumemptr) == mem_vmem_map.end())
		{
			debug_log << "[pause test4] a not found " << i << " ptr " << cumemptr << endl;
		}
		void* a = mem_vmem_map.find(cumemptr)->second;
		addr_s += cumems;
		QueryPerformanceCounter(&HookBeginStamp);
		mempoolBuffer = ::MapViewOfFile(mempoolMap, FILE_MAP_ALL_ACCESS, addr_s, addr_s + cumems, MAP_SIZE);//得到与共享内存映射的指针

		//string mempoolData = "1";
		strcpy((char*)mempoolBuffer, reinterpret_cast<char*>(a));//写入数据
		i++;
	}
	for (int i = 0; i < vmem_map.size(); i++)
	{
		cuEventDestroy(eventStart[i]);
		cuEventDestroy(eventEnd[i]);
	}
	cuEventDestroy(copyStart);
	cuEventDestroy(copyEnd);
	delete[]eventStart;
	delete[]eventEnd;
	//Sleep(4000);
	return 0;
}
int CopyHtoD()
{
	LARGE_INTEGER HookBeginStamp;
	QueryPerformanceCounter(&HookBeginStamp);
	debug_log << "[pause] begin 2, total " << vmem_map.size() << " iters " << endl;
	CUevent* eventStart;
	CUevent copyStart;
	CUevent* eventEnd;
	CUevent copyEnd;
	CUstream stream = 0;
	CUresult cuStatus;
	map<CUdeviceptr, size_t>::iterator iter;
	float ms, total_ms, total_ms2;
	int i = 0;
	eventStart = new CUevent[vmem_map.size()];
	eventEnd = new CUevent[vmem_map.size()];

	for (int i = 0; i < vmem_map.size(); i++)
	{
		cuStatus = cuEventCreate(&eventStart[i], CU_EVENT_BLOCKING_SYNC);
		if (cuStatus != CUDA_SUCCESS)
		{
			debug_log << "[test4] cuEventCreate failed 1 " << HookBeginStamp.QuadPart << " cuStatus " << cuStatus << endl;
		}
		cuStatus = cuEventCreate(&eventEnd[i], CU_EVENT_BLOCKING_SYNC);
		if (cuStatus != CUDA_SUCCESS)
		{
			debug_log << "[test4] cuEventCreate failed 2 " << HookBeginStamp.QuadPart << " cuStatus " << cuStatus << endl;
		}
	}
	cuStatus = cuEventCreate(&copyStart, CU_EVENT_BLOCKING_SYNC);
	if (cuStatus != CUDA_SUCCESS)
	{
		debug_log << "[test4] cuEventCreate failed 3 " << HookBeginStamp.QuadPart << " cuStatus " << cuStatus << endl;
	}
	cuStatus = cuEventCreate(&copyEnd, CU_EVENT_BLOCKING_SYNC);
	if (cuStatus != CUDA_SUCCESS)
	{
		debug_log << "[test4] cuEventCreate failed 4 " << HookBeginStamp.QuadPart << " cuStatus " << cuStatus << endl;
	}
	int total_size = 0;
	total_ms = 0;

	cuEventRecord(copyStart, stream);
	cuEventSynchronize(copyStart);//finish all previous jobs
	cuEventRecord(copyStart, stream);
	for (iter = vmem_map.begin(); iter != vmem_map.end(); iter++)
	{
		test_cumemptr = iter->first;
		test_cumems = iter->second;
		QueryPerformanceCounter(&HookBeginStamp);
		//debuglog << "[pause] DtoH iter " << i << " cuptr " << test_cumemptr << " size " << test_cumems << " B    at " << HookBeginStamp.QuadPart << endl;
		map<CUdeviceptr, void*>::iterator iter2 = mem_vmem_map.find(test_cumemptr);
		if (iter2 == mem_vmem_map.end())
		{
			debug_log << "[test4] HtoD map failed " << HookBeginStamp.QuadPart << endl;
		}
		void* a = iter2->second;
		test_cumemptr_addr = table_vmem_map.find(test_cumemptr)->second;
		cuEventRecord(eventStart[i], stream);
		cuStatus = cuMemcpyHtoD(test_cumemptr, a, test_cumems);
		if (cuStatus != CUDA_SUCCESS)
		{
			debug_log << "[test4] cuMemcpyHtoD failed " << HookBeginStamp.QuadPart << " cuStatus " << cuStatus << endl;
		}
		cuEventRecord(eventEnd[i], stream);

		i++;
	}
	//for (iter = vmem_map.begin(); iter != vmem_map.end(); iter++)
	//{
	//	map<CUdeviceptr, void*>::iterator iter2 = mem_vmem_map.find(test_cumemptr);
	//	if (iter2 == mem_vmem_map.end())
	//	{
	//		debuglog << "[test4] HtoD map failed " << HookBeginStamp.QuadPart << endl;
	//	}
	//	void* a = iter2->second;

	//	//free内存
	//	cuStatus = cuMemFreeHost(a);
	//	if (cuStatus != CUDA_SUCCESS)
	//	{
	//		debuglog << "[test4] ocuMemFreeHost failed " << HookBeginStamp.QuadPart << " cuStatus " << cuStatus << endl;
	//	}
	//	//free(a);
	//	i++;
	//}
	cuEventRecord(copyEnd, stream);
	cuEventSynchronize(copyEnd);
	i = 0;
	for (iter = vmem_map.begin(); iter != vmem_map.end(); iter++)
	{
		cuEventElapsedTime(&ms, eventStart[i], eventEnd[i]);
		total_ms += ms;
		QueryPerformanceCounter(&HookBeginStamp);
		//debuglog << "[pause] HtoD iter " << i << "    size " << iter->second << " B    takes " << ms << " ms   at " << HookBeginStamp.QuadPart << endl;
		i++;
	}
	cuEventElapsedTime(&ms, copyStart, copyEnd);
	debug_log << "[pause] load totally takes " << total_ms << " ms    actually: " << ms << " ms " << endl;
	for (int i = 0; i < vmem_map.size(); i++)
	{
		cuEventDestroy(eventStart[i]);
		cuEventDestroy(eventEnd[i]);
	}
	cuEventDestroy(copyStart);
	cuEventDestroy(copyEnd);
	delete[]eventStart;
	delete[]eventEnd;
	//free(a);
	//Sleep(2000);
	return 0;
}
CUresult CUDAAPI hkcuLaunchKernel(CUfunction f, unsigned int gridDimX, unsigned int gridDimY, unsigned int gridDimZ, unsigned int blockDimX, unsigned int blockDimY, unsigned int blockDimZ, unsigned int sharedMemBytes, CUstream hStream, void** kernelParams, void** extra)
{
	if (kernel_time.find(f) == kernel_time.end())
	{
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
		kernel_time.insert(pair<CUfunction, double>(f, elapsedTime));
		return ret;
	}
	double cur_time = kernel_time.find(f)->second;

	while (launch_available(cur_time))
	{
		Sleep(2);
	}
	AcquireSRWLockExclusive(&time_lock);
	timeslice -= cur_time;
	ReleaseSRWLockExclusive(&time_lock);

	Queryparam* queueparam = new Queryparam;
	cudaEventCreate(&queueparam->event1);
	cudaEventRecord(queueparam->event1, 0);
	CUresult ret = ocuLaunchKernel(f, gridDimX, gridDimY, gridDimZ, blockDimX, blockDimY, blockDimZ, sharedMemBytes, hStream, kernelParams, extra);
	kernelremainnum++;
	kernelremaintime += cur_time;
	cudaEventCreate(&queueparam->event2);
	cudaEventRecord(queueparam->event2, 0);
	queueparam->time = cur_time;

	kernelrecordqueue.push(queueparam);
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

CUresult CUDAAPI hkcuMemAlloc(CUdeviceptr* dptr, size_t bytesize)
{
	LARGE_INTEGER HookBeginStamp;
	QueryPerformanceCounter(&HookBeginStamp);
	CUresult ret = ocuMemAlloc(dptr, bytesize);
	if (train_prepare)
	{
		HANDLE hMap = NULL;
		hMap = OpenFileMapping(FILE_MAP_ALL_ACCESS, 0, L"trainpre");
		if (hMap) {
			debug_log << "[cuda]====== training prepared " << HookBeginStamp.QuadPart << endl;
			train_prepare = false;
		}
		return ret;
	}
	else if (model_init)
	{
		return ret;
	}
	else
	{
		HANDLE hMap = NULL;
		hMap = OpenFileMapping(FILE_MAP_ALL_ACCESS, 0, L"modelinit");
		if (hMap) {
			init_mempool();
			debug_log << "[cuda]====== model initiated " << HookBeginStamp.QuadPart << endl;
			model_init = true;
			return ret;
		}
		vmem_map.insert(pair<CUdeviceptr, size_t>(*dptr, bytesize));
		table_vmem_map.insert(pair<CUdeviceptr, CUdeviceptr*>(*dptr, dptr));
		if (vmem_map.size() != table_vmem_map.size()) debug_log << "[test1] two map not the same length, CUdeviceptr* or CUdeviceptr duplicated!" << endl;
		debug_log << "[cuda]====== cuMemAlloc hooked " << HookBeginStamp.QuadPart << " dptr " << dptr << " *dptr " << *dptr << " size " << bytesize << " vmem len " << vmem_map.size() << " table_vmem len " << table_vmem_map.size() << endl;
		return ret;
	}
}

CUresult CUDAAPI hkcuMemFree(CUdeviceptr dptr)
{
	//LARGE_INTEGER HookBeginStamp;
	//QueryPerformanceCounter(&HookBeginStamp);
	//map<CUdeviceptr, CUdeviceptr*>::iterator iter = table_vmem_map.find(dptr);
	//if (iter == table_vmem_map.end())
	//{
	//	debuglog << "[test4] hkcuMemFree table_vmem_map missed dptr " << dptr << endl;
	//}
	//else
	//{
	//	CUdeviceptr* dptraddr = table_vmem_map.find(dptr)->second;
	//	vmem_map.erase(dptr);
	//	table_vmem_map.erase(dptr);
	//}
	//debuglog << "[cuda]====== cuMemFree hooked " << HookBeginStamp.QuadPart << endl;
	CUresult ret = ocuMemFree(dptr);

	return ret;
}

CUresult CUDAAPI hkcuCtxCreate(CUcontext* pctx, unsigned int  flags, CUdevice dev)
{
	LARGE_INTEGER HookBeginStamp;
	QueryPerformanceCounter(&HookBeginStamp);
	debug_log << "[cuda]====== cuCtxCreate hooked " << HookBeginStamp.QuadPart << endl;
	CUresult ret = ocuCtxCreate(pctx, flags, dev);

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
		kernellaunchnumMap = OpenFileMapping(FILE_MAP_ALL_ACCESS, 0, L"kernellaunchnum");
		kernellaunchtimeMap = OpenFileMapping(FILE_MAP_ALL_ACCESS, 0, L"kernellaunchtime");
		kernelremainnumMap = OpenFileMapping(FILE_MAP_ALL_ACCESS, 0, L"kernelremainnum");
		kernelremaintimeMap = OpenFileMapping(FILE_MAP_ALL_ACCESS, 0, L"kernelremaintime");


		flipBuffer = MapViewOfFile(flipMap, FILE_MAP_ALL_ACCESS, 0, 0, 0);
		timeBuffer = MapViewOfFile(timeMap, FILE_MAP_ALL_ACCESS, 0, 0, 0);
		busyBuffer = MapViewOfFile(busyMap, FILE_MAP_ALL_ACCESS, 0, 0, 0);
		kernellaunchnumBuffer = ::MapViewOfFile(kernellaunchnumMap, FILE_MAP_ALL_ACCESS, 0, 0, 0);
		kernellaunchtimeBuffer = ::MapViewOfFile(kernellaunchtimeMap, FILE_MAP_ALL_ACCESS, 0, 0, 0);
		kernelremainnumBuffer = ::MapViewOfFile(kernelremainnumMap, FILE_MAP_ALL_ACCESS, 0, 0, 0);
		kernelremaintimeBuffer = ::MapViewOfFile(kernelremaintimeMap, FILE_MAP_ALL_ACCESS, 0, 0, 0);

		hook_log.open("D:\\CloudGaming\\Log\\hook_log.txt");
		debug_log.open("D:\\CloudGaming\\Log\\debug_log.txt");
		kernel_log.open("D:\\CloudGaming\\Log\\kernel_log.txt");
		kernel_unhooked_log.open("D:\\CloudGaming\\Log\\kernel_unhooked_log.txt");
		kernel_unregistered_log.open("D:\\CloudGaming\\Log\\kernel_unregistered_log.txt");
		kernel_avetime.open(KERNELAVG2);

		get_kernel_time(kernel_offline);
		CreateThread(NULL, NULL, flip_detection, NULL, 0, NULL);

		if (hook_log)MessageBoxA(0, "DLL injected", "step 4", 3);
		hook_log << "dll inject" << endl;
		DisableThreadLibraryCalls(hInstance);
		GetModuleFileNameA(hInstance, dlldir, 512);
		for (size_t i = strlen(dlldir); i > 0; i--) { if (dlldir[i] == '\\') { dlldir[i + 1] = 0; break; } }
		cuLaunchKerneladdr = (cuLaunchKernelt)getLibraryProcAddress("nvcuda.dll", "cuLaunchKernel");
		cuModuleGetFunctionaddr = (cuModuleGetFunctiont)getLibraryProcAddress("nvcuda.dll", "cuModuleGetFunction");
		cuMemAllocaddr = (cuMemAlloct)getLibraryProcAddress("nvcuda.dll", "cuMemAlloc_v2");
		cuMemFreeaddr = (cuMemFreet)getLibraryProcAddress("nvcuda.dll", "cuMemFree_v2");
		//cuCtxCreateaddr = (cuCtxCreatet)getLibraryProcAddress("C:\\Windows\\System32\\nvcuda.dll", "cuCtxCreate_v2");
		debug_log << "culaucnkernel addr " << cuLaunchKerneladdr << endl;
		debug_log << "cuMemAlloc addr " << cuMemAllocaddr << endl;
		debug_log << "cuMemFree addr " << cuMemFreeaddr << endl;
		hook_log << "culaucnkernel addr " << cuLaunchKerneladdr << endl;
		hook_log << "cuModuleGetFunction addr" << cuModuleGetFunctionaddr << endl;
		//debuglog << "cuCtxCreate addr " << cuCtxCreateaddr << endl;

		if (MH_Initialize() != MH_OK) hook_log << "initialize hook failed" << endl; else hook_log << "initialize hook sucess" << endl;
		if (MH_CreateHook((LPVOID)cuMemAllocaddr, hkcuMemAlloc, (LPVOID*)&ocuMemAlloc) != MH_OK) debug_log << "create hook2 failed" << endl;
		else debug_log << "create hook2 success" << endl;
		if (MH_CreateHook((LPVOID)cuMemFreeaddr, hkcuMemFree, (LPVOID*)&ocuMemFree) != MH_OK) debug_log << "create hook3 failed" << endl;
		else debug_log << "create hook3 success" << endl;
		//if (MH_CreateHook((LPVOID)cuCtxCreateaddr, hkcuCtxCreate, (LPVOID*)&ocuCtxCreate) != MH_OK) debuglog << "create hook4 failed" << endl;
		//else debuglog << "create hook4 success" << endl;
		if (MH_CreateHook((LPVOID)cuLaunchKerneladdr, hkcuLaunchKernel, (LPVOID*)&ocuLaunchKernel) != MH_OK) hook_log << "create cuLaunchKernel failed" << endl;
		else hook_log << "create cuLaunchKernel success" << endl;
		if (MH_CreateHook((LPVOID)cuModuleGetFunctionaddr, hkcuModuleGetFunction, (LPVOID*)&ocuModuleGetFunction) != MH_OK) hook_log << "create cuModuleGetFunction failed" << endl;
		else hook_log << "create cuModuleGetFunction success" << endl;

		if (MH_EnableHook((LPVOID)cuLaunchKerneladdr) != MH_OK) hook_log << "enable cuLaunchKernel failed" << endl;
		else hook_log << "enable cuLaunchKernel success" << endl;
		if (MH_EnableHook((LPVOID)cuModuleGetFunctionaddr) != MH_OK) hook_log << "enable cuModuleGetFunction failed" << endl;
		else hook_log << "enable cuModuleGetFunction success" << endl;
		if (MH_EnableHook((LPVOID)cuMemAllocaddr) != MH_OK) debug_log << "enable hook2 failed" << endl;
		else debug_log << "enable hook2 success" << endl;
		if (MH_EnableHook((LPVOID)cuMemFreeaddr) != MH_OK) debug_log << "enable hook3 failed" << endl;
		else debug_log << "enable hook3 success" << endl;
		//if (MH_EnableHook((LPVOID)cuCtxCreateaddr) != MH_OK) debuglog << "enable hook4 failed" << endl;
		//else debuglog << "enable hook4 success" << endl;

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
		if (MH_DisableHook((LPVOID)&cuMemAllocaddr) != MH_OK) { return 1; }
		if (MH_DisableHook((LPVOID)&cuMemFreeaddr) != MH_OK) { return 1; }
		//if (MH_DisableHook((LPVOID)&cuCtxCreateaddr) != MH_OK) { return 1; }
		if (MH_Uninitialize() != MH_OK) { return 1; }
		break;
	}

	return TRUE;
}
