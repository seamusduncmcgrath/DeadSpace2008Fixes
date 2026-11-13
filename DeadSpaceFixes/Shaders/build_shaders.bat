@echo off
rem Compiles FXAA.hlsl into the two headers FXAA.cpp embeds. Run it after changing FXAA.hlsl.
rem Uses fxc from the newest Windows 10/11 SDK; set FXC to override.
setlocal
cd /d "%~dp0"

if not defined FXC (
	for /f "delims=" %%d in ('dir /b /ad /o-n "%ProgramFiles(x86)%\Windows Kits\10\bin\10.*" 2^>nul') do (
		if not defined FXC if exist "%ProgramFiles(x86)%\Windows Kits\10\bin\%%d\x64\fxc.exe" set "FXC=%ProgramFiles(x86)%\Windows Kits\10\bin\%%d\x64\fxc.exe"
	)
)
if not defined FXC (
	echo Couldn't find fxc.exe, install the Windows SDK or set FXC
	exit /b 1
)

rem Replaces EdgeAAFast: bb_texture in s0, g_halfPixelOffset in c1
"%FXC%" /nologo /T ps_3_0 /E main /O3 /D FXAA_INPUT=bb_texture /D FXAA_HALF_PIXEL_REGISTER=c1 /Vn g_FxaaEdgeAAFastPS /Fh FXAA_EdgeAAFast.h FXAA.hlsl || exit /b 1

rem Replaces DeferredEdgeAA: ColorTexture in s0, g_halfPixelOffset in c2
"%FXC%" /nologo /T ps_3_0 /E main /O3 /D FXAA_INPUT=ColorTexture /D FXAA_HALF_PIXEL_REGISTER=c2 /Vn g_FxaaDeferredEdgeAAPS /Fh FXAA_DeferredEdgeAA.h FXAA.hlsl || exit /b 1

echo Built FXAA_EdgeAAFast.h and FXAA_DeferredEdgeAA.h

rem Supersampling downscale: one vertex shader plus a pixel shader per tap count, joined into one header
"%FXC%" /nologo /T vs_3_0 /E VSMain /O3 /Vn g_SupersamplingVS /Fh Supersampling_VS.tmp Supersampling.hlsl || exit /b 1
"%FXC%" /nologo /T ps_3_0 /E PSMain /O3 /D TAPS=2 /Vn g_SupersamplingPS2 /Fh Supersampling_PS2.tmp Supersampling.hlsl || exit /b 1
"%FXC%" /nologo /T ps_3_0 /E PSMain /O3 /D TAPS=3 /Vn g_SupersamplingPS3 /Fh Supersampling_PS3.tmp Supersampling.hlsl || exit /b 1
"%FXC%" /nologo /T ps_3_0 /E PSMain /O3 /D TAPS=4 /Vn g_SupersamplingPS4 /Fh Supersampling_PS4.tmp Supersampling.hlsl || exit /b 1
copy /b /y Supersampling_VS.tmp + Supersampling_PS2.tmp + Supersampling_PS3.tmp + Supersampling_PS4.tmp Supersampling.h >nul || exit /b 1
del Supersampling_VS.tmp Supersampling_PS2.tmp Supersampling_PS3.tmp Supersampling_PS4.tmp

echo Built Supersampling.h
