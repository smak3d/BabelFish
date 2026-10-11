@echo off
rem ---------------------------------------------------------------------------
rem Сборка сервера с CUDA (build-cuda/).
rem
rem nvcc на Windows не умеет в MinGW-g++, поэтому для этого дерева нужен MSVC
rem из VS Build Tools. vcvars64 поднимает окружение MSVC, а каталоги nvcc
rem добавляем в PATH сами — vcvars про CUDA не знает, и без этого CMake не
rem находит компилятор. В CUDA 13.x x64-DLL (cublas и т.п.) лежат в bin\x64,
rem а не в bin — оба каталога нужны и при сборке, и при запуске.
rem
rem Использование:
rem   build-cuda.bat                     собрать всё
rem   build-cuda.bat --target BabelFishServer    только сервер
rem ---------------------------------------------------------------------------
setlocal

set "PATH=C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v13.4\bin\x64;C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v13.4\bin;%PATH%"
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" >nul 2>&1

if not exist "%~dp0build-cuda\CMakeCache.txt" (
    echo [build-cuda] first configure...
    cmake -S "%~dp0." -B "%~dp0build-cuda" -G Ninja -DCMAKE_BUILD_TYPE=Release ^
        -DBABELFISH_WITH_CUDA=ON ^
        -DCUDAToolkit_ROOT="C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v13.4" ^
        -DGGML_CUDA_ARCHITECTURES=75;120
    if errorlevel 1 exit /b 1
)

cmake --build "%~dp0build-cuda" --parallel %*
if errorlevel 1 exit /b 1

rem DLL ggml/llama/whisper по умолчанию собираются в build-cuda\bin, а exe —
rem в build-cuda\server: без каталога в PATH запуск падает с 0xC0000135.
rem Копируем их рядом с exe, чтобы сервер запускался как есть. CRT тоже
rem кладём сюда — на машине без установленного VC++ Redist он не найдётся.
set "BINDLL=%~dp0build-cuda\bin"
set "OUTDIR=%~dp0build-cuda\server"
if exist "%BINDLL%\ggml.dll" (
    copy /y "%BINDLL%\ggml*.dll"   "%OUTDIR%" >nul
    copy /y "%BINDLL%\llama.dll"   "%OUTDIR%" >nul
    copy /y "%BINDLL%\whisper.dll" "%OUTDIR%" >nul
    copy /y "%VCToolsRedistDir%x64\Microsoft.VC143.CRT\*.dll" "%OUTDIR%" >nul 2>&1
    echo [build-cuda] runtime DLLs copied to %OUTDIR%
)

exit /b %errorlevel%
