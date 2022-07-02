# Repo Structure
```
|---- PilotFish                 
    |---- KernelDLLInject       # training jobs inject program 
    |---- KernelHook            # training jobs hook program
    |---- RenderDLLInject       # games inject program 
    |---- RenderHook            # games hook program
    |---- script                # scripts 
```
Compile KernelHook and RenderHook to get dlls. Compile KernelDLLInject and RenderDLLInject to get 2 inject programs. They are used to inject the dlls into target processes. The script is used to run offline profiling and co-location experiments.
# Getting Started Instructions
1. Make sure that your system is windows 10. Install CUDA, PyTorch and Red Dead Redemption 2.
2. Use Visual studio to compile the whole project.
3. Edit the path of training jobs, dlls and inject programs according to your environment. 
4. Training job profile: Set dl_dll in the script to be 'off' and use it to profile the training job. Then set dl_dll in the script to be 'full' and use it fully profile the training job.
5. Co-location: Set dl_dll in the script to be 'pf'. Then launch the game and use RenderDLLInject to inject into the target game. When the game enters the benchmark, just run the script.

Before fully evaluation, you may need to know different versions of KernelHook and RenderHook.
# Specifications about KernelHook and RenderHook
The co-location version is specified in line 16 in Pilotfish/KernelHook/main.h and Pilotfish/RenderHook/main.h. Different versions are specified as below:
1. OFF: The parameter is used to profile training jobs offline. It only needs to be executed once and then outputs a kernel_avetime.txt which records all the kernel names and average time. The kernel_avetime.txt  will be used later and should be copied to KERNELAVG1 specified in main.h.
2. FULL: The parameter is used to fully profile training jobs offline. It is recommended to run this FULL version more than 2 times on the training job. (Not all kernels are profiled during the first round. ) The output in KERNELAVG2 specified in main.h should be copied to KERNELAVG1.
3. FPS: This version adjusts the number of kernels launched according to the real-time FPS.   
4. CSPEED: Constant speed, namely launching constant number of kernels per frame. You can specify the number in line 5 in main_CSPEED.h.
5. SOFT: PilotFish version, which does not introduce the checkpoint mechanism.
6. PF: PilotFish version with the checkpoint mechanism.


# Detailed Instructions
Suppose the whole project has been compiled, then you only need to use the kernel-inject.bat in the script folder to run the experiments. 
1. Edit the path of training jobs, dlls and inject programs according to your environment. 
2. Training job profile: Set dl_dll in the script to be 'off' and use it to profile the training job. Then set dl_dll in the script to be 'full' and use it fully profile the training job.
3. Co-location: Set dl_dll in the script to be the desired co-location version. Then launch the game and use RenderDLLInject to inject into the target game. When the game enters the benchmark, just run the script.

# Demo Video


https://user-images.githubusercontent.com/35565792/176988876-851f0fcc-91fe-4e6a-a3a1-e3fe3c0ebfb0.mp4


The left column is PilotFish, the middle one is game standalone, and the right one is co-location without throttling.

# Notes
1. The process name of games includes AshesEscalation_DX12.exe, RDR2.exe, HITMAN3.exe, SOTTR.exe, F1_2021.exe.
2. For most games, you should set the rendering engine as directx12 in settings, or they will use directx11 or vulkan default.
3. It is better to disable cudnn in training jobs to reduce the rounds needed in kernel profiles.
4. The version of CUDA and correlated header files may affect the compilation. You should check them in the main.h of RenderHook and KernelHook.
5. The injection of games should be done manually when the game starts running benchmarks, because the states of games can not be detected.
