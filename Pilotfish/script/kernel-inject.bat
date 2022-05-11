set dl=lstm
@REM resnet vgg mobile lstm
set dl_dll=off
@REM off full fps cspeed soft pf
set dl_dir=Test5_resnet

if %dl%==resenet (
    set dl_dir=resnet
) else if %dl%==vgg (
    set dl_dir=vggnet
) else if %dl%==mobile (
    set dl_dir=mobilenet
)

set script_path=D:\github\Pilotfish\Pilotfish\script
set pydir_path=D:\GPU\training\pytorch_classification\%dl_dir%\
if %dl%==lstm (
    set pydir_path=D:\GPU\training\lstm\
)
set py_path=%pydir_path%train.py
set dll_path=%script_path%\dl_dll\%dl_dll%.dll
set dll_path=D:\github\Pilotfish\Pilotfish\KernelHook\x64\Debug\KernelHook.dll
echo %py_path%
cd %script_path%


.\KernelDLLInject.exe python %py_path% %pydir_path% %dll_path%
