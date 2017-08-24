@echo off
set WSOCK_TRACE=%CD%/wsock_trace.appveyor

if %1. ==  build. goto :build
if %1. NEQ init.  exit /b 0

echo Generating wsock_trace.appveyor...
echo #                                    > wsock_trace.appveyor
echo # This file was generated from %0.  >> wsock_trace.appveyor
echo #                                   >> wsock_trace.appveyor
echo [core]                              >> wsock_trace.appveyor
echo trace_level            = 2          >> wsock_trace.appveyor
echo trace_indent           = 2          >> wsock_trace.appveyor
echo trace_caller           = 1          >> wsock_trace.appveyor
echo trace_report           = 1          >> wsock_trace.appveyor
echo trace_time             = relative   >> wsock_trace.appveyor
echo callee_level           = 1          >> wsock_trace.appveyor
echo cpp_demangle           = 1          >> wsock_trace.appveyor
echo short_errors           = 1          >> wsock_trace.appveyor
echo use_full_path          = 1          >> wsock_trace.appveyor
echo use_toolhlp32          = 1          >> wsock_trace.appveyor
echo dump_select            = 1          >> wsock_trace.appveyor
echo dump_hostent           = 1          >> wsock_trace.appveyor
echo dump_protoent          = 1          >> wsock_trace.appveyor
echo dump_servent           = 1          >> wsock_trace.appveyor
echo dump_nameinfo          = 1          >> wsock_trace.appveyor
echo dump_wsaprotocol_info  = 1          >> wsock_trace.appveyor
echo dump_wsanetwork_events = 1          >> wsock_trace.appveyor
echo dump_data              = 1          >> wsock_trace.appveyor
echo max_data               = 5000       >> wsock_trace.appveyor
echo max_displacement       = 100        >> wsock_trace.appveyor
echo exclude                = htons      >> wsock_trace.appveyor
echo exclude                = htonl      >> wsock_trace.appveyor
echo.                                    >> wsock_trace.appveyor
echo [geoip]                             >> wsock_trace.appveyor
echo enable               = 1            >> wsock_trace.appveyor
echo use_generated        = 0            >> wsock_trace.appveyor
echo max_days             = 10           >> wsock_trace.appveyor
echo geoip4_file          = %CD%\geoip   >> wsock_trace.appveyor
echo geoip6_file          = %CD%\geoip6  >> wsock_trace.appveyor
echo ip2location_bin_file =              >> wsock_trace.appveyor
echo.                                    >> wsock_trace.appveyor
echo [idna]                              >> wsock_trace.appveyor
echo enable   = 1                        >> wsock_trace.appveyor
echo winidn   = 0                        >> wsock_trace.appveyor
echo codepage = 0                        >> wsock_trace.appveyor

::
:: Get the IP2Location code.
::
md IP2Location
git clone https://github.com/chrislim2888/IP2Location-C-Library.git IP2Location
echo /* Dummy IP2Location config.h */ > IP2Location\config.h
goto :end

:build
::
:: Setup MSVC environment.
::
call "c:\Program Files\Microsoft SDKs\Windows\v7.1\Bin\SetEnv.cmd" /x86

set INCLUDE=%INCLUDE%;%CD%\IP2Location\libIP2Location
set COLUMNS=120

::
:: To bypass a C-99 issue in IP2Location/libIP2Location/IP2Loc_DBInterface.c:
::
set CL=-D__powerpc64__

cd src
nmake -nologo -f Makefile.vc6 USER=AppVeyor
if errorlevel exit /b 1

test.exe

:end