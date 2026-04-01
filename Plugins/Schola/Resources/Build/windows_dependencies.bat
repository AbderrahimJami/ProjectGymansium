@echo off
:: From Apache 2.0 Licensed https://github.com/vizor-games/InfraworldRuntime/blob/master/Setup.bat
:: Modifications Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.
::#####################################VARS#############################################################################
set SCRIPT_FOLDER=%cd%

set GRPC_ROOT=C:\tmp\grpc
set GRPC_INCLUDE_DIR=%SCRIPT_FOLDER%\..\..\Source\ThirdParty\Grpc\Win64\include
set GRPC_LIBRARIES_DIR=%SCRIPT_FOLDER%\..\..\Source\ThirdParty\Grpc\Win64\lib
set GRPC_PROGRAMS_DIR=%SCRIPT_FOLDER%\..\tools

set CMAKE_BUILD_DIR=%GRPC_ROOT%\.build
set UE_ROOT=C:\Program Files\Epic Games\UE_5.5
set REMOTE_ORIGIN=https://github.com/grpc/grpc.git
set BRANCH=v1.53.0

set ZLIB_VERSION=1.3
set OPENSSL_VERSION=1.1.1t

set CLEAN_GRPC=0


:: original branch was v1.23.x
::#####################################VARS#############################################################################


:MAIN
    echo ">>>>>>>>>> cleaning"
    CALL :CLEAN || GOTO :ABORT

    IF %CLEAN_GRPC% == 0 (echo ">>>>>>>>>> clone git")
    IF %CLEAN_GRPC% == 0 (CALL :UPDATE_GRPC || GOTO :ABORT)
    
    echo ">>>>>>>>>> making build configs"
    CALL :CMAKE || GOTO :ABORT

    echo ">>>>>>>>>> building"
    CALL :BUILD || GOTO :ABORT

    echo ">>>>>>>>>> copying files"
    CALL :COPY_ALL || GOTO :ABORT

    :: Roll over to the good case
    :GRACEFULEXIT
    CALL :CLEANUP
    echo Build done!
    goto :eof

    :ABORT
    echo Aborted...
    CALL :CLEANUP
    goto :eof
goto :eof
    

:GET_UE_ROOT
    IF "%UE_ROOT%" == "" (echo "UE_ROOT directory does not exist, please set correct UE_ROOT via SET UE_ROOT=<PATH_TO_UNREAL_ENGINE_FOLDER>" && GOTO :ABORT)
EXIT /B %ERRORLEVEL%

:CLEAN
    IF EXIST "%CMAKE_BUILD_DIR%" (rmdir "%CMAKE_BUILD_DIR%" /s /q)
    IF EXIST "%GRPC_INCLUDE_DIR%" (rmdir "%GRPC_INCLUDE_DIR%" /s /q)
    IF EXIST "%GRPC_LIBRARIES_DIR%" (rmdir "%GRPC_LIBRARIES_DIR%" /s /q)
    IF EXIST "%GRPC_PROGRAMS_DIR%" (rmdir "%GRPC_PROGRAMS_DIR%" /s /q)
EXIT /B %ERRORLEVEL%

:UPDATE_GRPC
    IF EXIST "%GRPC_ROOT%" (rmdir "%GRPC_ROOT%" /s /q)
    (call git clone "%REMOTE_ORIGIN%" "%GRPC_ROOT%" && cd "%GRPC_ROOT%") || EXIT /B %ERRORLEVEL%
    git fetch
    git checkout -f
    git checkout tags/%BRANCH%
    git submodule update --init
EXIT /B %ERRORLEVEL%

:: ABSL, CARES and RE2 building as modules are new additions
:CMAKE
    if NOT EXIST "%CMAKE_BUILD_DIR%" (mkdir "%CMAKE_BUILD_DIR%")
    pushd "%CMAKE_BUILD_DIR%"
    call cmake .. -G "Visual Studio 17 2022" -A x64 ^
        -DCMAKE_POLICY_VERSION_MINIMUM=3.5 ^
        -DCMAKE_CXX_STANDARD=17 ^
        -DABSL_PROPAGATE_CXX_STD=ON ^
        -DCMAKE_CXX_STANDARD_LIBRARIES="Crypt32.Lib User32.lib Advapi32.lib" ^
        -DCMAKE_BUILD_TYPE=Release ^
        -DCMAKE_CONFIGURATION_TYPES=Release ^
        -Dprotobuf_BUILD_TESTS=OFF ^
        -DgRPC_BUILD_TESTS=OFF ^
        -DgRPC_BUILD_CSHARP_EXT=OFF ^
        -DgRPC_ABSL_PROVIDER=module ^
        -DgRPC_CARES_PROVIDER=module ^
        -DgRPC_RE2_PROVIDER=module ^
        -DgRPC_ZLIB_PROVIDER=package ^
        -DZLIB_INCLUDE_DIR="%UE_ROOT%\Engine\Source\ThirdParty\zlib\%ZLIB_VERSION%\include" ^
        -DZLIB_LIBRARY_DEBUG="%UE_ROOT%\Engine\Source\ThirdParty\zlib\%ZLIB_VERSION%\lib\Win64\Release\zlibstatic.lib" ^
        -DZLIB_LIBRARY_RELEASE="%UE_ROOT%\Engine\Source\ThirdParty\zlib\%ZLIB_VERSION%\lib\Win64\Release\zlibstatic.lib" ^
        -DgRPC_SSL_PROVIDER=package ^
        -DLIB_EAY_LIBRARY_DEBUG="%UE_ROOT%\Engine\Source\ThirdParty\OpenSSL\%OPENSSL_VERSION%\Lib\Win64\VS2015\Debug\libcrypto.lib" ^
        -DLIB_EAY_LIBRARY_RELEASE="%UE_ROOT%\Engine\Source\ThirdParty\OpenSSL\%OPENSSL_VERSION%\Lib\Win64\VS2015\Release\libcrypto.lib" ^
        -DLIB_EAY_DEBUG="%UE_ROOT%\Engine\Source\ThirdParty\OpenSSL\%OPENSSL_VERSION%\Lib\Win64\VS2015\Debug\libcrypto.lib" ^
        -DLIB_EAY_RELEASE="%UE_ROOT%\Engine\Source\ThirdParty\OpenSSL\%OPENSSL_VERSION%\Lib\Win64\VS2015\Release\libcrypto.lib" ^
        -DOPENSSL_INCLUDE_DIR="%UE_ROOT%\Engine\Source\ThirdParty\OpenSSL\%OPENSSL_VERSION%\include\Win64\VS2015" ^
        -DSSL_EAY_DEBUG="%UE_ROOT%\Engine\Source\ThirdParty\OpenSSL\%OPENSSL_VERSION%\Lib\Win64\VS2015\Release\libssl.lib" ^
        -DSSL_EAY_LIBRARY_DEBUG="%UE_ROOT%\Engine\Source\ThirdParty\OpenSSL\%OPENSSL_VERSION%\Lib\Win64\VS2015\Release\libssl.lib" ^
        -DSSL_EAY_LIBRARY_RELEASE="%UE_ROOT%\Engine\Source\ThirdParty\OpenSSL\%OPENSSL_VERSION%\Lib\Win64\VS2015\Release\libssl.lib" ^
        -DSSL_EAY_RELEASE="%UE_ROOT%\Engine\Source\ThirdParty\OpenSSL\%OPENSSL_VERSION%\Lib\Win64\VS2015\Release\libssl.lib" ^
        -DgRPC_PROTOBUF_PROVIDER=module
    :: Return to previous dir
    popd
EXIT /B %ERRORLEVEL%

:BUILD
    pushd "%CMAKE_BUILD_DIR%"
    call cmake --build . --config Release --clean-first -j8
    :: Return to previous dir
    popd
EXIT /B %ERRORLEVEL%


:COPY_ALL
    CALL :COPY_HEADERS
    CALL :COPY_LIBRARIES
    CALL :COPY_PROGRAMS
EXIT /B %ERRORLEVEL%

:COPY_HEADERS
    echo ">>>>>>>>>> copy headers"
    robocopy "%GRPC_ROOT%\include" "%GRPC_INCLUDE_DIR%" /E
    robocopy "%GRPC_ROOT%\third_party\protobuf\src\google" "%GRPC_INCLUDE_DIR%\google" /E
    robocopy "%GRPC_ROOT%\third_party\abseil-cpp\absl" "%GRPC_INCLUDE_DIR%\absl" /E
EXIT /B %ERRORLEVEL%

:COPY_LIBRARIES
    echo ">>>>>>>>>> copy libraries"
    if NOT EXIST "%GRPC_LIBRARIES_DIR%" (mkdir "%GRPC_LIBRARIES_DIR%")
    :: robocopy "%CMAKE_BUILD_DIR%\Release" "%GRPC_LIBRARIES_DIR%" *.lib /R:0 /S
    for /R %CMAKE_BUILD_DIR% %%f in (*.lib) do copy "%%f" %GRPC_LIBRARIES_DIR%\
EXIT /B %ERRORLEVEL%

:COPY_PROGRAMS
    echo ">>>>>>>>>> copy programs"
    robocopy "%CMAKE_BUILD_DIR%\Release" "%GRPC_PROGRAMS_DIR%" *.exe /R:0 /S
    :: copy "%UE_ROOT%\Engine\Plugins\Experimental\NNERuntimeORTCpu\Source\ThirdParty\onnxruntime\Dependencies\Protobuf\bin\Win64\protoc.exe" "%GRPC_PROGRAMS_DIR%\protoc.exe"
    copy "%CMAKE_BUILD_DIR%\third_party\protobuf\Release\protoc.exe" "%GRPC_PROGRAMS_DIR%\protoc.exe"

EXIT /B %ERRORLEVEL%


:REMOVE_USELESS_LIBRARIES
    echo ">>>>>>>>>> remove useless libraries"
    ::del "%GRPC_LIBRARIES_DIR%\grpc_csharp_ext.lib"
    ::del "%GRPC_LIBRARIES_DIR%\gflags_static.lib"
    ::del "%GRPC_LIBRARIES_DIR%\gflags_nothreads_static.lib"
    ::del "%GRPC_LIBRARIES_DIR%\benchmark.lib"
goto :eof

:CLEANUP
    echo  ">>>>>>>>>> Cleaning Up"
    cd "%SCRIPT_FOLDER%"
EXIT /B %ERRORLEVEL%