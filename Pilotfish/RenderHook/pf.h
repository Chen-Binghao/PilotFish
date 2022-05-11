LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
char stateText[50] = "123";
char timeText[50] = "123";
char appText[50] = "123";
char rendertimeText[50] = "123";
int dx12Thread();
bool prend = false;
bool InitOnce = true;
bool record_cmd = false;
bool gamelog = false;
bool flip = false;
bool paused = false;
bool firstlog = true;
double fps = 0;
int frametime = 0;
int curr_fence = 1;
int framenum = 0;
int cmdnum = 0;
LONGLONG pause_time = 0;
HANDLE last_drawevent;
ofstream presentlog;
ofstream debuglog;
ofstream renderlog;
FILE* rendertime_f;
LARGE_INTEGER injectStamp;
LARGE_INTEGER counterStamp;
LARGE_INTEGER stopStamp;
LARGE_INTEGER resumeStamp;


//string busyMapName("busy");
string busyData = "1";
LPVOID busyBuffer = NULL;
HANDLE busyMap;
//string flipMapName("flip");
string flipData = "0";
LPVOID flipBuffer = NULL;
HANDLE flipMap;
//string timeMapName("time");
string timeData = "1.3245";
LPVOID timeBuffer = NULL;
HANDLE timeMap;
string kernellaunchnumData = "0";
LPVOID kernellaunchnumBuffer = NULL;
HANDLE kernellaunchnumMap;
char kernellaunchnumB[256] = { 0 };
string kernellaunchtimeData = "0";
LPVOID kernellaunchtimeBuffer = NULL;
HANDLE kernellaunchtimeMap;
char kernellaunchtimeB[256] = { 0 };
string kernelremaintimeData = "0";
LPVOID kernelremaintimeBuffer = NULL;
HANDLE kernelremaintimeMap;
char kernelremaintimeB[256] = { 0 };
string kernelremainnumData = "0";
LPVOID kernelremainnumBuffer = NULL;
HANDLE kernelremainnumMap;
char kernelremainnumB[256] = { 0 };

ID3D12Fence* fence;
ID3D12Device* d3dDevice = NULL;
ComPtr<ID3D12CommandQueue>			pICMDQueue;
ComPtr<ID3D12RootSignature>			pIRootSignature;
ComPtr<ID3D12PipelineState>			pIPipelineState;
ComPtr<ID3D12CommandAllocator>		pICMDAlloc;
ComPtr<ID3D12GraphicsCommandList>	pICMDList;
ComPtr<ID3DBlob> pIBlobVertexShader;
ComPtr<ID3DBlob> pIBlobPixelShader;
LARGE_INTEGER Lastrendercomplete;
struct Queryparam
{
	HANDLE event1;
	HANDLE event2;
	int fnum;
	int cnum;
	int type;
	int extra1;
	LARGE_INTEGER entertime;
	ID3D12Fence* fence1;
};
queue <Queryparam*> testp;
queue <Queryparam*> testq;
queue<LARGE_INTEGER> presentqueue;
Queryparam* BeginParam, * EndParam;
class MyMutex
{
public:
	MyMutex()
	{
		InitializeCriticalSection(&m_criticalSection);
	}
	~MyMutex()
	{
		DeleteCriticalSection(&m_criticalSection);
	}
	static MyMutex& instance()
	{
		static MyMutex inst;
		return inst;
	}
	void lock()
	{
		EnterCriticalSection(&m_criticalSection);
	}
	void unlock()
	{
		LeaveCriticalSection(&m_criticalSection);
	}

protected:
	CRITICAL_SECTION            m_criticalSection;
};

typedef long(__stdcall* Present12) (IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags);
Present12 oPresent12 = NULL;

typedef void(__stdcall* ExecuteCommandLists12)(
	ID3D12CommandQueue* This,
	UINT              NumCommandLists,
	ID3D12CommandList* const* ppCommandLists);
ExecuteCommandLists12 oExecuteCommandLists = NULL;

void setbusy(bool busy)
{
	if (busy)busyData = "1";
	else busyData = "0";
	//LARGE_INTEGER HookBeginStamp;
	//QueryPerformanceCounter(&HookBeginStamp);
	//renderlog << "[mem] setbusy " << busyData << endl;
	strcpy((char*)busyBuffer, busyData.c_str());
	return;
}

void setflip()
{

	flip = !flip;
	if (flip) flipData = "1";
	else flipData = "0";
	//LARGE_INTEGER HookBeginStamp;
	//QueryPerformanceCounter(&HookBeginStamp);
	//renderlog << "[mem] setflip " << flipData << endl;
	strcpy((char*)flipBuffer, flipData.c_str());
	return;
}

void settime(float timeslice)
{
	timeData = to_string(timeslice);
	//LARGE_INTEGER HookBeginStamp;
	//QueryPerformanceCounter(&HookBeginStamp);
	//renderlog << "[mem] settime " << timeData << endl;
	strcpy((char*)timeBuffer, timeData.c_str());
	return;
}

int Getkernel_launchnum()
{
	strcpy(kernellaunchnumB, (char*)kernellaunchnumBuffer);
	bool update = true;
	int kernel_num = atoi(string(kernellaunchnumB).c_str());
	return kernel_num;
}

float Getkernel_launchtime()
{
	strcpy(kernellaunchtimeB, (char*)kernellaunchtimeBuffer);
	bool update = true;
	float kernel_num = atof(string(kernellaunchtimeB).c_str());
	return kernel_num;
}

int Getkernel_remainnum()
{
	strcpy(kernelremainnumB, (char*)kernelremainnumBuffer);
	bool update = true;
	int kernel_num = atoi(string(kernelremainnumB).c_str());
	return kernel_num;
}

float Getkernel_remaintime()
{
	strcpy(kernelremaintimeB, (char*)kernelremaintimeBuffer);
	bool update = true;
	double kernel_num = atof(string(kernelremaintimeB).c_str());
	return kernel_num;
}

byte GetGpuRate_byte() {
	nvmlReturn_t result;
	unsigned int device_count, i;
	// First initialize NVML library
	result = nvmlInit();

	result = nvmlDeviceGetCount(&device_count);
	if (NVML_SUCCESS != result)
	{
		return 0;
	}

	for (i = 0; i < device_count; i++)
	{
		nvmlDevice_t device;
		char name[NVML_DEVICE_NAME_BUFFER_SIZE];
		nvmlPciInfo_t pci;
		result = nvmlDeviceGetHandleByIndex(i, &device);
		if (NVML_SUCCESS != result) {
			return 0;
		}
		result = nvmlDeviceGetName(device, name, NVML_DEVICE_NAME_BUFFER_SIZE);
		if (NVML_SUCCESS != result) {
			return 0;
		}
		nvmlUtilization_t utilization;
		result = nvmlDeviceGetUtilizationRates(device, &utilization);
		if (NVML_SUCCESS != result)
		{
			return 0;
		}
		return ((byte)utilization.gpu);
	}
	return 0;
}


long __stdcall hkPresent12(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags)
{
	if (InitOnce)
	{
		InitOnce = false;
		//get device
		if (SUCCEEDED(pSwapChain->GetDevice(__uuidof(ID3D12Device), (void**)&d3dDevice)))
		{
			pSwapChain->GetDevice(__uuidof(d3dDevice), (void**)&d3dDevice);
			d3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE,
				IID_PPV_ARGS(&fence));
			D3D12_ROOT_SIGNATURE_DESC stRootSignatureDesc =
			{
				0
				, nullptr
				, 0
				, nullptr
				, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT
			};

			ComPtr<ID3DBlob> pISignatureBlob;
			ComPtr<ID3DBlob> pIErrorBlob;

			D3D12SerializeRootSignature(
				&stRootSignatureDesc
				, D3D_ROOT_SIGNATURE_VERSION_1
				, &pISignatureBlob
				, &pIErrorBlob);

			d3dDevice->CreateRootSignature(0
				, pISignatureBlob->GetBufferPointer()
				, pISignatureBlob->GetBufferSize()
				, IID_PPV_ARGS(&pIRootSignature));
			D3D12_INPUT_ELEMENT_DESC stInputElementDescs[] =
			{
				{
					"POSITION"
					, 0
					, DXGI_FORMAT_R32G32B32A32_FLOAT
					, 0
					, 0
					, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA
					, 0
				},
				{
					"COLOR"
					, 0
					, DXGI_FORMAT_R32G32B32A32_FLOAT
					, 0
					, 16
					, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA
					, 0
				}
			};
			UINT nCompileFlags = 0;
			D3DCompileFromFile(L"D:\\github\\rendertime-profile-tool\\shaders.hlsl"
				, nullptr
				, nullptr
				, "VSMain"
				, "vs_5_0"
				, nCompileFlags
				, 0
				, &pIBlobVertexShader
				, nullptr);
			D3DCompileFromFile(L"D:\\github\\rendertime-profile-tool\\shaders.hlsl"
				, nullptr
				, nullptr
				, "PSMain"
				, "ps_5_0"
				, nCompileFlags
				, 0
				, &pIBlobPixelShader
				, nullptr);
			D3D12_GRAPHICS_PIPELINE_STATE_DESC stPSODesc = {};

			stPSODesc.InputLayout = { stInputElementDescs, _countof(stInputElementDescs) };
			stPSODesc.pRootSignature = pIRootSignature.Get();
			stPSODesc.VS.pShaderBytecode = pIBlobVertexShader->GetBufferPointer();
			stPSODesc.VS.BytecodeLength = pIBlobVertexShader->GetBufferSize();
			stPSODesc.PS.pShaderBytecode = pIBlobPixelShader->GetBufferPointer();
			stPSODesc.PS.BytecodeLength = pIBlobPixelShader->GetBufferSize();

			stPSODesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
			stPSODesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;

			stPSODesc.BlendState.AlphaToCoverageEnable = FALSE;
			stPSODesc.BlendState.IndependentBlendEnable = FALSE;
			stPSODesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

			stPSODesc.DepthStencilState.DepthEnable = FALSE;
			stPSODesc.DepthStencilState.StencilEnable = FALSE;

			stPSODesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

			stPSODesc.NumRenderTargets = 1;
			stPSODesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;

			stPSODesc.SampleMask = UINT_MAX;
			stPSODesc.SampleDesc.Count = 1;

			d3dDevice->CreateGraphicsPipelineState(&stPSODesc, IID_PPV_ARGS(&pIPipelineState));
			d3dDevice->CreateCommandAllocator(
				D3D12_COMMAND_LIST_TYPE_DIRECT
				, IID_PPV_ARGS(&pICMDAlloc));

			d3dDevice->CreateCommandList(
				0
				, D3D12_COMMAND_LIST_TYPE_DIRECT
				, pICMDAlloc.Get()
				, pIPipelineState.Get()
				, IID_PPV_ARGS(&pICMDList));
		}
	}
	LARGE_INTEGER HookBeginStamp;
	QueryPerformanceCounter(&HookBeginStamp);
	presentqueue.push(HookBeginStamp);
	if (presentqueue.size() > 61) presentqueue.pop();

	LARGE_INTEGER PresentStamp;
	QueryPerformanceCounter(&PresentStamp);
	renderlog << "frame " << framenum << " pre begin " << PresentStamp.QuadPart << endl;
	//setbusy(true);

	framenum++;
	if (record_cmd)
	{
		record_cmd = false;
		debuglog << "[d3d12]====== enter queue 1 " << HookBeginStamp.QuadPart << " type " << EndParam->type << " frame " << EndParam->fnum << " cmd num " << EndParam->cnum << endl;
		testq.push(EndParam);
		EndParam = new Queryparam;
	}
	auto ret = oPresent12(pSwapChain, SyncInterval, Flags);

	prend = true;
	return ret;
}



void __stdcall hkExecuteCommandLists12(
	ID3D12CommandQueue* This,
	UINT              NumCommandLists,
	ID3D12CommandList* const* ppCommandLists)
{
	MyMutex::instance().lock();

	LARGE_INTEGER HookBeginStamp;
	QueryPerformanceCounter(&HookBeginStamp);

	debuglog << "[d3d12]====== ExecuteCommandLists hooked " << HookBeginStamp.QuadPart << " cmdqueue " << This << " type " << This->GetDesc().Type << " cmd " << ppCommandLists << " frame " << framenum << " cmd num " << cmdnum << endl;
	if (This->GetDesc().Type == D3D12_COMMAND_LIST_TYPE_DIRECT && prend)
	{
		prend = false;
		pICMDList->Reset(pICMDAlloc.Get(), pIPipelineState.Get());

		pICMDList->SetGraphicsRootSignature(pIRootSignature.Get());
		pICMDList->SetPipelineState(pIPipelineState.Get());
		pICMDList->DrawInstanced(0, 0, 0, 0);
		pICMDList->Close();
		ID3D12CommandList* ppCommandLists[] = { pICMDList.Get() };


		oExecuteCommandLists(This, _countof(ppCommandLists), ppCommandLists);
		uint64_t set = curr_fence;
		This->Signal(fence, set);
		++curr_fence;
		HANDLE event = CreateEventEx(nullptr, FALSE, FALSE, EVENT_ALL_ACCESS);
		//debuglog << "set fence " << curr_fence - 1 << endl;
		if (fence->GetCompletedValue() < set)
		{
			//debuglog << "set event " << curr_fence - 1 << endl;
			fence->SetEventOnCompletion(set, event);
			QueryPerformanceCounter(&HookBeginStamp);
			BeginParam = new Queryparam;
			BeginParam->event2 = event;
			BeginParam->fnum = framenum;
			BeginParam->cnum = cmdnum;
			BeginParam->type = This->GetDesc().Type;
			BeginParam->entertime = HookBeginStamp;
			BeginParam->extra1 = 0;
			testq.push(BeginParam);
			debuglog << "[d3d12]====== enter queue 0 " << HookBeginStamp.QuadPart << " cmdqueue " << This << " type " << This->GetDesc().Type << " cmd " << ppCommandLists << " frame " << framenum << " cmd num " << cmdnum << endl;
		}

	}
	oExecuteCommandLists(This, NumCommandLists, ppCommandLists);

	if (This->GetDesc().Type == D3D12_COMMAND_LIST_TYPE_DIRECT)
	{

		HANDLE event = CreateEventEx(nullptr, FALSE, FALSE, EVENT_ALL_ACCESS);
		uint64_t set = curr_fence;
		This->Signal(fence, set);
		++curr_fence;
		//debuglog << "set fence " << curr_fence - 1 << endl;
		if (fence->GetCompletedValue() < set)
		{
			//debuglog << "set event " << curr_fence - 1 << endl;
			fence->SetEventOnCompletion(set, event);
			last_drawevent = event;
			QueryPerformanceCounter(&HookBeginStamp);
			EndParam->event2 = event;
			EndParam->fnum = framenum;
			EndParam->cnum = cmdnum;
			EndParam->type = This->GetDesc().Type;
			EndParam->entertime = HookBeginStamp;
			EndParam->extra1 = 1;
			record_cmd = true;
			//renderlog << framenum << " " << a.num << " " << This->GetDesc().Type << " " << a.type << endl;
			//CreateThread(NULL, NULL, querystamptomb, a, 0, NULL);
			cmdnum++;
		}
	}

	MyMutex::instance().unlock();
	return;
}

DWORD WINAPI queryrenderstate(LPVOID lpParam)
{
	double render = 0;
	LARGE_INTEGER BeginStamp, EndStamp;
	QueryPerformanceCounter(&BeginStamp);
	QueryPerformanceCounter(&EndStamp);
	while (true)
	{
		if (testq.empty())
		{
			Sleep(1);
		}
		else
		{
			Queryparam* a = testq.front();
			LARGE_INTEGER t = a->entertime;
			HANDLE waitevent2 = a->event2;
			int frame_num = a->fnum;
			int cmd_num = a->cnum;
			int queuetype = a->type;
			LARGE_INTEGER Stamp2;
			QueryPerformanceCounter(&Stamp2);
			WaitForSingleObject(waitevent2, 60);
			LARGE_INTEGER Stamp3;
			QueryPerformanceCounter(&Stamp3);
			//renderlog << "frame " << frame_num << " b/e " << a->extra1 << " cmd " << cmd_num << " type " << queuetype << " finish render at " << (Stamp3.QuadPart) << " wait for " << (Stamp3.QuadPart - Stamp2.QuadPart) << " wait from " << (Stamp2.QuadPart) << " exe at " << t.QuadPart << endl;
			if (a->extra1)
			{
				EndStamp = Stamp3;
				fps = 6e8 / 2 / (double)(presentqueue.back().QuadPart - presentqueue.front().QuadPart);
				render = (double)(EndStamp.QuadPart - BeginStamp.QuadPart) / 1e4;
				sprintf_s(rendertimeText, "rendertime  %.3f fps %.2f", render, fps);
				QueryPerformanceCounter(&counterStamp);
				float realtime = (counterStamp.QuadPart - injectStamp.QuadPart - pause_time) / 1e7;
				double kernel_launchtime = Getkernel_launchtime();
				int kernel_launchnum = Getkernel_launchnum();
				double kernel_remaintime = Getkernel_remaintime();
				int kernel_remainnum = Getkernel_remainnum();
				if (kernel_remaintime > 1)
				{
					setbusy(true);
				}
				else
				{
					setbusy(false);
				}
				if (gamelog)
				{
					fprintf(rendertime_f, "%f,%d,%f,%f,%d, ,%f,%d,%f,%d\n", realtime, framenum, render, fps, GetGpuRate_byte(),
						kernel_launchtime, kernel_launchnum, kernel_remaintime, kernel_remainnum);
					setbusy(false);
					settime((16.6 - render));
					setflip();
					debuglog << "good " << 16.6 - render << " flip state " << flip << endl;
				}
			}
			else
			{
				setbusy(false);
				BeginStamp = Stamp3;
			}
			delete a;
			testq.pop();
			//setbusy(false);
			//if (!fps_down) settime((16 - render) * func());
			//else settime(0);
			//setflip();
		}
	}


	return 1;
}

DWORD WINAPI Create_window(LPVOID lpParam)
{
	static TCHAR szAppName[] = TEXT("debug_w");
	HWND hwnd;
	MSG msg;
	WNDCLASS wndclass;
	wndclass.style = CS_HREDRAW | CS_VREDRAW;
	wndclass.cbClsExtra = 0;
	wndclass.cbWndExtra = 0;
	wndclass.hInstance = GetModuleHandle(NULL);
	wndclass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wndclass.hCursor = LoadCursor(NULL, IDC_ARROW);
	wndclass.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
	wndclass.lpszMenuName = NULL;
	wndclass.lpfnWndProc = WndProc;
	wndclass.lpszClassName = szAppName;
	if (!RegisterClass(&wndclass))
	{
		MessageBox(NULL, TEXT("This program requires Windows NT!"), szAppName, MB_ICONERROR);
		return 0;
	};
	hwnd = CreateWindow(szAppName,      // window class name
		TEXT("game"),   // window caption
		WS_OVERLAPPEDWINDOW, // window style
		50,// initial x position
		50,// initial y position
		500,// initial x size
		200,// initial y size
		NULL, // parent window handle
		NULL, // window menu handle
		GetModuleHandle(NULL), // program instance handle
		NULL);
	CreateWindow(TEXT("BUTTON"), TEXT("begin log"), WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 40, WIDTH * 4,
		100, WIDTH, hwnd, (HMENU)IDC_BUTTON1, (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);
	CreateWindow(TEXT("BUTTON"), TEXT("end log"), WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 40, WIDTH * 5,
		100, WIDTH, hwnd, (HMENU)IDC_BUTTON2, (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);
	SetTimer(hwnd, 1, 100, NULL);
	ShowWindow(hwnd, SW_SHOWNORMAL);
	UpdateWindow(hwnd);

	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
	return msg.wParam;
}


LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)

{
	HDC hdc;
	PAINTSTRUCT ps;
	RECT rect;
	int wmId, wmEvent;
	static int counter = 0;
	float realtime = 0;
	static double kernel_launchtime = 0;
	static int kernel_launchnum = 0;
	static double kernel_remaintime = 0;
	static int kernel_remainnum = 0;
	switch (message)
	{
	case WM_CREATE:
		//PlaySound(TEXT("HelloWin.wav"),NULL,SND_FILENAME|SND_ASYNC);
		return 0;

	case   WM_PAINT:
		hdc = BeginPaint(hwnd, &ps);
		TextOutA(hdc, 0, 0, (LPCSTR)stateText, 40);
		TextOutA(hdc, 0, WIDTH, (LPCSTR)rendertimeText, 40);
		TextOutA(hdc, 0, WIDTH * 2, (LPCSTR)timeText, 40);
		TextOutA(hdc, 0, WIDTH * 3, (LPCSTR)appText, 40);

		//TextOutW(hdc, 0, 0, (LPCWSTR)szText, 10);
		EndPaint(hwnd, &ps);
		return 0;

	case   WM_DESTROY:
		PostQuitMessage(0);
		return 0;

	case   WM_TIMER:
		QueryPerformanceCounter(&counterStamp);
		realtime = (counterStamp.QuadPart - injectStamp.QuadPart - pause_time) / 1e7;
		sprintf_s(stateText, "time  %.3f s   log_state  %d", realtime, gamelog);
		kernel_launchtime = Getkernel_launchtime();
		kernel_launchnum = Getkernel_launchnum();
		kernel_remaintime = Getkernel_remaintime();
		kernel_remainnum = Getkernel_remainnum();
		sprintf_s(timeText, "time  %d lt %.3f ln %d", counter, kernel_launchtime, kernel_launchnum);
		sprintf_s(appText, "rt %.3f rn %d", kernel_remaintime, kernel_remainnum);
		counter++;
		RECT rect2;
		GetClientRect(hwnd, &rect2);
		UpdateWindow(hwnd);
		RedrawWindow(hwnd, &rect2, nullptr, RDW_INVALIDATE | RDW_UPDATENOW);
		return 0;
	case WM_COMMAND:
	{
		wmId = LOWORD(wParam);
		wmEvent = HIWORD(wParam);
		switch (wmId)
		{
		case IDC_BUTTON1:
			gamelog = true;
			if (paused)
			{
				QueryPerformanceCounter(&resumeStamp);
				pause_time += resumeStamp.QuadPart - stopStamp.QuadPart;
				paused = false;
			}
			if (firstlog)
			{
				QueryPerformanceCounter(&counterStamp);
				pause_time += counterStamp.QuadPart - injectStamp.QuadPart;
				firstlog = false;
			}
			counter = 0;
			paused = false;
			break;
		case IDC_BUTTON2:
			gamelog = false;
			paused = true;
			setbusy(false);
			QueryPerformanceCounter(&stopStamp);
			counter = 0;
			break;
		default:
			return DefWindowProc(hwnd, message, wParam, lParam);
		}
	}
	}
	return DefWindowProc(hwnd, message, wParam, lParam);

}




int dx12Thread()
{
	presentlog << "begin allocate memory" << endl;
	busyMap = ::CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0,
		MAP_SIZE,
		L"busy");

	flipMap = ::CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0,
		MAP_SIZE,
		L"flip");

	timeMap = ::CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0,
		MAP_SIZE,
		L"time");

	kernellaunchnumMap = ::CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0,
		MAP_SIZE,
		L"kernellaunchnum");

	kernellaunchtimeMap = ::CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0,
		MAP_SIZE,
		L"kernellaunchtime");

	kernelremainnumMap = ::CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0,
		MAP_SIZE,
		L"kernelremainnum");

	kernelremaintimeMap = ::CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0,
		MAP_SIZE,
		L"kernelremaintime");

	if (busyMap)
		presentlog << "memory1 allocate success" << endl;
	else
		presentlog << "memory1 allocate fail" << endl;

	if (flipMap)
		presentlog << "memory2 allocate success" << endl;
	else
		presentlog << "memory2 allocate fail" << endl;

	if (timeMap)
		presentlog << "memory3 allocate success" << endl;
	else
		presentlog << "memory3 allocate fail" << endl;

	busyBuffer = ::MapViewOfFile(busyMap, FILE_MAP_ALL_ACCESS, 0, 0, VIEW_SIZE);
	flipBuffer = ::MapViewOfFile(flipMap, FILE_MAP_ALL_ACCESS, 0, 0, VIEW_SIZE);
	timeBuffer = ::MapViewOfFile(timeMap, FILE_MAP_ALL_ACCESS, 0, 0, VIEW_SIZE);
	kernellaunchnumBuffer = ::MapViewOfFile(kernellaunchnumMap, FILE_MAP_ALL_ACCESS, 0, 0, VIEW_SIZE);
	kernellaunchtimeBuffer = ::MapViewOfFile(kernellaunchtimeMap, FILE_MAP_ALL_ACCESS, 0, 0, VIEW_SIZE);
	kernelremainnumBuffer = ::MapViewOfFile(kernelremainnumMap, FILE_MAP_ALL_ACCESS, 0, 0, VIEW_SIZE);
	kernelremaintimeBuffer = ::MapViewOfFile(kernelremaintimeMap, FILE_MAP_ALL_ACCESS, 0, 0, VIEW_SIZE);
	strcpy((char*)kernellaunchnumBuffer, kernellaunchtimeData.c_str());
	strcpy((char*)kernellaunchtimeBuffer, kernellaunchtimeData.c_str());
	strcpy((char*)kernelremainnumBuffer, kernellaunchtimeData.c_str());
	strcpy((char*)kernelremaintimeBuffer, kernellaunchtimeData.c_str());
	setbusy(false);

	Sleep(4000);
	if (dx12::init(dx12::RenderType::D3D12) == dx12::Status::Success)
	{
		debuglog << "init d3d12 success " << endl;
		MH_Initialize();
		MH_CreateHook((LPVOID)dx12::getMethodsTable()[140], hkPresent12, (LPVOID*)&oPresent12);

		MH_CreateHook((LPVOID)dx12::getMethodsTable()[54], hkExecuteCommandLists12, (LPVOID*)&oExecuteCommandLists);





		MH_EnableHook((LPVOID)dx12::getMethodsTable()[140]);

		MH_EnableHook((LPVOID)dx12::getMethodsTable()[54]);
	}
	else
	{
		debuglog << "init d3d12 failed !" << endl;
	}

	presentlog << "1" << endl;
	return 0;
}


//=========================================================================================================================//

BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD fdwReason, LPVOID)
{
	DisableThreadLibraryCalls(hInstance);

	switch (fdwReason)
	{
	case DLL_PROCESS_ATTACH:
		timeBeginPeriod(1);
		BeginParam = new Queryparam;
		EndParam = new Queryparam;
		QueryPerformanceCounter(&injectStamp);
		QueryPerformanceCounter(&counterStamp);
		QueryPerformanceCounter(&stopStamp);
		QueryPerformanceCounter(&resumeStamp);
		presentlog.open("d:\\CloudGaming\\Log2\\fence-present.txt");
		debuglog.open("d:\\CloudGaming\\Log2\\debug.txt");
		renderlog.open("d:\\CloudGaming\\Log2\\renderlog.txt");
		rendertime_f = fopen(LOGDIR, "w+");
		fprintf(rendertime_f, "realtime,framenum,rendertime,fps,GPUUtil, ,launchtime,launchnum,remaintime,remainnum,fpskernelnum\n");
		CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)Create_window, NULL, 0, NULL);
		debuglog << "create window thread" << endl;
		CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)dx12Thread, NULL, 0, NULL);
		debuglog << "create dx12Thread hook thread" << endl;
		DisableThreadLibraryCalls(hInstance);
		GetModuleFileNameA(hInstance, dlldir, 512);
		CreateThread(NULL, NULL, queryrenderstate, NULL, 0, NULL);
		for (size_t i = strlen(dlldir); i > 0; i--) { if (dlldir[i] == '\\') { dlldir[i + 1] = 0; break; } }
		break;
	case DLL_PROCESS_DETACH: // A process unloads the DLL.
		timeEndPeriod(1);
		MessageBoxA(0, "DLL removed", "step 4", 3);
		if (MH_DisableHook((LPVOID)dx12::getMethodsTable()[140]) != MH_OK) { return 1; }
		if (MH_DisableHook((LPVOID)dx12::getMethodsTable()[54]) != MH_OK) { return 1; }

		if (MH_Uninitialize() != MH_OK) { return 1; }
		break;
	}

	return TRUE;
}