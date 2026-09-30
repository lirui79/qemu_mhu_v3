#!/usr/bin/env sh

UT_WORKDIR=$(cd $(dirname $0); pwd)
# GTEST_SOURCE_DIR=$UT_WORKDIR/gtest
build_dir="build"
lcov_dir="lcov"
gcovr_dir="gcovr"

# ---------------- build gtest from source code -------------------------
# cd $GTEST_SOURCE_DIR
# if [ ! -d "$build_dir" ]; then
#    mkdir $build_dir
#    echo "\e[36m-- Make gtest build/ directory \e[0m"
# else
#    rm -rf $build_dir/*
#    echo "\e[36m-- Empty gtest build/ directory \e[0m"
# fi
# cd $GTEST_SOURCE_DIR/$build_dir
# cmake ../
# make
# if [ $? -ne "0" ]; then
#    echo "\e[31m-- Build gtest failed!!! \e[0m"
#    exit
# else
#    echo "\e[1;36m-- Build gtest finished!!! \e[0m"
# fi

#cp lib/*.a $UT_WORKDIR/lib
# ------------------------------------------------------------------------

cd $UT_WORKDIR
if [ ! -d "$build_dir" ]; then
   mkdir $build_dir
   echo "\e[36m-- Make UT build/ directory \e[0m"
else
   rm -rf $build_dir/*
   echo "\e[36m-- Empty UT build/ directory \e[0m"
fi

cd $UT_WORKDIR/$build_dir
cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ../ #for cppcheck
make
if [ $? -ne "0" ]; then
   echo "\e[31m-- Build UT failed!!! \e[0m"
   exit
else
   echo "\e[1;36m-- Build UT finished!!! \e[0m"
fi
   
./out/ut
if [ $? -ne "0" ]; then
   echo "\e[1;31m-- UT Failed!!! \e[0m"
   exit
else
   echo "\e[1;36m-- UT Succeeded!!! \e[0m"
fi

cppcheck --project=compile_commands.json > cppcheck.log # cppcheck report

if [ ! -d "$lcov_dir" ]; then
   mkdir $lcov_dir
   echo "\e[36m-- Make UT build/lcov directory \e[0m"
else
   rm -rf $lcov_dir/*
   echo "\e[36m-- Empty UT build/lcov directory \e[0m"
fi
if [ ! -d "$gcovr_dir" ]; then
   mkdir $gcovr_dir
   echo "\e[36m-- Make UT build/gcovr directory \e[0m"
else
   rm -rf $gcovr_dir/*
   echo "\e[36m-- Empty UT build/gcovr directory \e[0m"
fi

cd $UT_WORKDIR/$build_dir/$lcov_dir # lcov report
lcov --capture --directory $UT_WORKDIR/$build_dir --output-file coverage.info
genhtml coverage.info --output-directory ./

cd $UT_WORKDIR/$build_dir/$gcovr_dir # gcovr report
gcovr -r $UT_WORKDIR --html --html-details -o coverage.html

