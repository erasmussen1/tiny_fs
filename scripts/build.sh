#!/bin/bash
#
set -o nounset
# set -x

WORKING_DIR="$(cd $(dirname $0) && cd .. && pwd)"


# Script version
SCRIPT_VERSION="1.00.06.00"

# default out of source directory
buildDir="build"

# Keep the gtest colors when run from ctest
export GTEST_COLOR=1

NP=1
if [[ -e /proc/cpuinfo ]]
then
    NP=$(cat /proc/cpuinfo | grep ^processor | wc -l)
fi

die()
{
    cat <<LAB_DIE
======================================
 $*
======================================
LAB_DIE

    exit 1
}

makeBuildDir()
{
    [[ $1 -eq 1 ]] && [[ -d $buildDir ]] && rm -rf $buildDir
    [[ $1 -eq 2 ]] && [[ ! -d $buildDir ]] && mkdir -p $buildDir
}

generateBuildSystem()
{
    makeBuildDir 2

    echo "======================"
    for a in $*
    do
        echo $a
    done
    echo "======================"

    pushd $buildDir
    cmake $* ..
    [[ $? -ne 0 ]] && die "ERR: failed to generate cmake "

    popd
}

buildApp()
{
    makeBuildDir 2

    pushd ${buildDir}
    make -j ${NP}
    [[ $? -ne 0 ]] && die "ERR: failed to build project $(pwd)"
    popd

}

buildPackage()
{
    makeBuildDir 2

    pushd $buildDir
    make package
    popd

    return 0
}

runUnitTest()
{
    makeBuildDir 2

    pushd $buildDir
    ctest -j1 -VV
    [[ $? -ne 0 ]] && die "ERR: unit-tests failed $(pwd)"
    popd
}

usage()
{
cat <<LAB_USAGE
$(basename "$0")  v$SCRIPT_VERSION

usage:
    -G    Run all unittest
    -p    Build install package
    -r    Release build

    -h    Show this menu


Advanced:
    -d    Delete build directory
    -c    Generate build system
    -e    Build project
    -l    Build with shared libraries
    -b    Build benchmarks
    -g    Run all unit-tests
    -C    Build with Clang compiler
    -s    Build with sanitizer flags on
    -S    Build and run cppcheck
    -T    Build and run clang-tidy
    -P    Build property based tests


LAB_USAGE
}


generateBuildSystemFlag=0
runUnitTestFlag=0
buildPackageFlag=0

clearBuildFlag=0
checkSoftware=0
buildOnlyFlag=0
buildForBenchmarkFlag=0
clangToolchainFlag=0
toolchain=" "
buildRelease=" "
buildType=" "
sanitizer=" "
fuzzer=" "
benchmark=" "
unittest=" "
runtidy=" "
runcppcheck=" "
runpropertycheck=" "

while getopts "dcegbCsfrTSPGprlhu" opt
do
    case $opt in
        d)
            clearBuildFlag=1
            ;;
        c)
            generateBuildSystemFlag=1
            ;;
        e)
            buildOnlyFlag=1
            ;;
        g)
            runUnitTestFlag=1
            unittest="-DBUILD_UNITTEST=ON"
            ;;
        b)
            buildForBenchmarkFlag=1
            benchmark="-DBUILD_BENCHMARK=ON"
            ;;
        C)
            toolchain="-DCMAKE_TOOLCHAIN_FILE=../cmake/toolchain_clang.cmake"
            ;;
        s)
            sanitizer="-DENABLE_SANITIZER=ON"
            ;;
        f)
            fuzzer="-DBUILD_FUZZER=ON"
            ;;
        r)
            buildRelease="-DRELEASE_BUILD=ON"
            ;;
        T)
            runtidy="-DRUN_CLANG_TIDY=ON"
            ;;
        S)
            runcppcheck="-DENABLE_CPPCHECK=ON"
            ;;
        P)
            runpropertycheck="-DENABLE_RAPIDCHECK=ON"
            ;;
        l)
            buildType="-DBUILD_SHARED=ON"
            ;;
        G)
            clearBuildFlag=1
            generateBuildSystemFlag=1
            clangToolchainFlag=1
            buildOnlyFlag=1
            runUnitTestFlag=1
            unittest="-DBUILD_UNITTEST=ON"
            ;;
        p)
            buildPackageFlag=1
            ;;
        h|u|?)
            usage
            exit 1
            ;;
    esac
done



pushd ${WORKING_DIR}

if [[ $# -eq 0 ]]
then
    clearBuildFlag=1
    generateBuildSystemFlag=1
    buildOnlyFlag=1
fi

makeBuildDir $clearBuildFlag

[[ $generateBuildSystemFlag -ne 0 ]] && generateBuildSystem "${toolchain} ${buildRelease} ${buildType} ${sanitizer} ${unittest} ${benchmark} ${fuzzer} ${runtidy} ${runcppcheck} ${runpropertycheck}"
[[ $buildOnlyFlag -ne 0 ]] && buildApp
[[ $runUnitTestFlag -ne 0 ]] && runUnitTest
[[ ${buildPackageFlag} -ne 0 ]] && buildPackage

popd
