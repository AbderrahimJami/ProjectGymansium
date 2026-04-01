#!/bin/bash
# From Apache 2.0 Licensed https://github.com/vizor-games/InfraworldRuntime/blob/master/Setup.sh
# Modifications Copyright (C) 2024 Advanced Micro Devices, Inc. All rights reserved.
######################################VARS#############################################################################

# Exit on errors if any
set -e

###############################################################################
# Should be defined as an environment variable, will be v1.3.x otherwise
branch=${branch:-v1.53.0}
protobuf_branch=${protobuf_branch:-v3.21.12}
# original branch was v1.23.x
ZLIB_VERSION="1.3"
OPENSSL_VERSION="1.1.1t"

clean=${clean:-true}

VAR_GIT_BRANCH=$branch
VAR_PROTO_BRANCH=$protobuf_branch
VAR_CLEAR_REPO=$clean

REMOTE_ORIGIN="https://github.com/grpc/grpc.git"

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
GRPC_FOLDER_NAME=grpc
GRPC_ROOT="/tmp/${GRPC_FOLDER_NAME}"
BUILD_DIR="${GRPC_ROOT}/.build"

#will likely neet to install libtool-bin cmake and pkg-config
DEPS=(git automake autoconf libtool cmake strip pkg-config)

# Linux needs an existing UE installation. Default to UE_5.4 in Home directory
UE_ROOT=${UE_ROOT:-"~/UE_5.4"}

if [ ! -d "$UE_ROOT" ]; then
    echo "UE_ROOT directory ${UE_ROOT} does not exist, please set correct UE_ROOT"
    exit 1
fi;

# Compute arch string using uname
UNAME_MACH=$(echo $(uname -m) | tr '[:upper:]' '[:lower:]')
UNAME_OS=$(echo $(uname) | tr '[:upper:]' '[:lower:]')
UNAME_ARCH="${UNAME_MACH}-unknown-${UNAME_OS}-gnu"

### todo: make v22_clang-16.0.6-centos7 into variable
UE_CLANG_ROOT="${UE_ROOT}/Engine/Extras/ThirdPartyNotUE/SDKs/HostLinux/Linux_x64/v22_clang-16.0.6-centos7/${UNAME_ARCH}"
###############################################################################

OPENSSL_LIB_DIR="${UE_ROOT}/Engine/Source/ThirdParty/OpenSSL/${OPENSSL_VERSION}/lib/Unix/${UNAME_ARCH}"
OPENSSL_INCLUDE="${UE_ROOT}/Engine/Source/ThirdParty/OpenSSL/${OPENSSL_VERSION}/include/Unix"
PROTOBUF_DIR="${UE_ROOT}/Engine/Plugins/Experimental/NNERuntimeORTCpu/Source/ThirdParty/onnxruntime/Dependencies/Protobuf"

echo "SCRIPT_DIR=${SCRIPT_DIR}"
echo "GRPC_ROOT=${GRPC_ROOT}"

# Check if all tools are installed
for i in ${DEPS[@]}; do
    if [ ! "$(which ${i})" ];then
       echo "${i} not found, install via 'apt-get install ${i}'" && exit 1
    fi
done

# Check if ran under Linux
if [ $(uname) != 'Linux' ]; then
    echo "Can not work under $(uname) operating system, should be Linux! Exiting..."
    exit 1
fi;

# Clone or pull
if [ ! -d "$GRPC_ROOT" ]; then
    echo "Cloning repo into ${GRPC_ROOT}"
    git clone $REMOTE_ORIGIN $GRPC_ROOT
else
    [[ ${VAR_CLEAR_REPO} ]] && cd $GRPC_ROOT && git merge --abort || true; git clean -fdx && git checkout -f .
    echo "Pulling repo"
    (cd $GRPC_ROOT && git fetch)
fi

echo "Checking out branch ${VAR_GIT_BRANCH}"
(cd $GRPC_ROOT && git checkout tags/$VAR_GIT_BRANCH || true)


if [ "$VAR_GIT_BRANCH" \< "v1.63.0" ]; then
    echo "${VAR_GIT_BRANCH} is less than v1.63.0 patching systemd.cmake"
    (cd $GRPC_ROOT && git checkout tags/v1.63.0 cmake/systemd.cmake || true)
fi

# Update submodules
(cd $GRPC_ROOT && git submodule update --init)

if [ "$VAR_CLEAR_REPO" = "true" ]; then
    echo "Cleaning repo and submodules because VAR_CLEAR_REPO is set to ${VAR_CLEAR_REPO}"
    (cd $GRPC_ROOT && make clean)
    (cd $GRPC_ROOT && git clean -fdx)
    (cd $GRPC_ROOT && git submodule foreach git clean -fdx)
    (cd $GRPC_ROOT/third_party/protobuf && git checkout tags/${VAR_PROTO_BRANCH} ) #set protobuf version to match ONNX Unreal
elif [ "$VAR_CLEAR_REPO" = "false" ]; then
    echo "Cleaning is not needed!"
else
    echo "Undefined behaviour, VAR_CLEAR_REPO is ${VAR_CLEAR_REPO}!"
    exit 1
fi


HEADERS_DIR="${SCRIPT_DIR}/../../Source/ThirdParty/gRPC/Linux/include"
LIBS_DIR="${SCRIPT_DIR}/../../Source/ThirdParty/gRPC/Linux/lib"

# (re)-create headers directory
if [ -d "$HEADERS_DIR" ]; then
    printf '%s\n' "Removing old $HEADERS_DIR"
    rm -rf "$HEADERS_DIR"
fi

if [ -d "$LIBS_DIR" ]; then
    printf '%s\n' "Removing old $LIBS_DIR"
    rm -rf "$LIBS_DIR"
fi

mkdir $HEADERS_DIR -p
mkdir $LIBS_DIR -p


cp -R "${GRPC_ROOT}/include"/* $HEADERS_DIR
cp -R "${GRPC_ROOT}/third_party/protobuf/src/google" "${HEADERS_DIR}"
cp -R "${GRPC_ROOT}/third_party/abseil-cpp/absl" "${HEADERS_DIR}"

export HAS_SYSTEM_CARES=false
export HAS_SYSTEM_PROTOBUF=false
export HAS_SYSTEM_ZLIB=false

LIBCXX_UE_DIR="${UE_ROOT}/Engine/Source/ThirdParty/Unix/LibCxx/include" # the original is Linux, but changing to Unix solves the problem
LIBC_UE_DIR="${UE_ROOT}/Engine/Source/ThirdParty/Unix/LibCxx/include" # the original is Linux, but changing to Unix solves the problem
LIBCXX_UE_LIB_DIR="${UE_ROOT}/Engine/Source/ThirdParty/Unix/LibCxx/lib/Unix/${UNAME_ARCH}"

IGNORED_WARNINGS="-Wno-deprecated-non-prototype -Wno-expansion-to-defined -Wno-error -Wno-unused-command-line-argument"


export LDFLAGS="-L${LIBCXX_UE_LIB_DIR} -L${OPENSSL_LIB_DIR} -fuse-ld=lld"
export LDLIBS="-lc++ -lc++abi -lc"
export CXXFLAGS="-o2 -std=c++17 -fPIC -nostdinc++ -stdlib=libc++ --sysroot=${UE_CLANG_ROOT} ${IGNORED_WARNINGS} -I${LIBCXX_UE_DIR} -I${LIBCXX_UE_DIR}/c++/v1 -L${LIBCXX_UE_LIB_DIR} -L${LIBCXX_UE_LIB_DIR}/libc++.a -L${LIBCXX_UE_LIB_DIR}/libc++abi.a -lm -lc -lgcc_s -lgcc "
export CFLAGS="-o2 -fPIC --sysroot=${UE_CLANG_ROOT} ${IGNORED_WARNINGS} "

mkdir -p ${BUILD_DIR}

(cd ${BUILD_DIR} && cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=./install \
    -DCMAKE_C_COMPILER="${UE_CLANG_ROOT}/bin/clang" \
    -DCMAKE_CXX_COMPILER="${UE_CLANG_ROOT}/bin/clang++" \
    -DgRPC_SSL_PROVIDER=package \
    -DOPENSSL_INCLUDE_DIR=${OPENSSL_INCLUDE} \
    -DOPENSSL_SSL_LIBRARY=${OPENSSL_LIB_DIR}/libssl.a \
    -DOPENSSL_CRYPTO_LIBRARY=${OPENSSL_LIB_DIR}/libcrypto.a \
    -Dprotobuf_BUILD_TESTS=OFF \
    -DgRPC_USE_SYSTEMD=OFF \
    -DgRPC_PROTOBUF_PROVIDER=module
    )


(cd ${BUILD_DIR} && cmake --build . --config Release --clean-first -j8)

find "${BUILD_DIR}" -name '*.a' -exec cp -f '{}' $LIBS_DIR ";"
echo "Build done! headers are in ${HEADERS_DIR}, and libs are in ${LIBS_DIR}"