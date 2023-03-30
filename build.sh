#!/bin/bash

function compile() 
{

export ARCH=arm64
export KBUILD_BUILD_HOST="nekokawai"
export KBUILD_BUILD_USER="ninja"
export USE_CCACHE=1
ccache -M 5G
git clone --depth=1 https://github.com/sarthakroy2002/android_prebuilts_clang_host_linux-x86_clang-6443078 $HOME/toolchains/clang
git clone --depth=1 https://github.com/LineageOS/android_prebuilts_gcc_linux-x86_aarch64_aarch64-linux-android-4.9 $HOME/toolchains/los-4.9-64
git clone --depth=1 https://github.com/LineageOS/android_prebuilts_gcc_linux-x86_arm_arm-linux-androideabi-4.9 $HOME/toolchains/los-4.9-32

[ -d "out" ] && rm -rf out || mkdir -p out

make O=out ARCH=arm64 spaced_defconfig menuconfig

PATH="$HOME/toolchains//clang/bin:${PATH}:$HOME/toolchains//los-4.9-32/bin:${PATH}:$HOME/toolchains//los-4.9-64/bin:${PATH}" \
make -j$(nproc --all) O=out \
                      ARCH=arm64 \
                      CC="ccache clang" \
                      CLANG_TRIPLE=aarch64-linux-gnu- \
                      CROSS_COMPILE="$HOME/toolchains/los-4.9-64/bin/aarch64-linux-android-" \
                      CROSS_COMPILE_ARM32="$HOME/toolchains/los-4.9-32/bin/arm-linux-androideabi-" \
                      CONFIG_NO_ERROR_ON_MISMATCH=y \
                      -j16
}

function zupload()
{
rm -rf AnyKernel
git clone --depth=1 git@github.com:Carckv2/AnyKernel3.git AnyKernel
cp out/arch/arm64/boot/Image.gz-dtb AnyKernel
cd AnyKernel
zip -r9 nekokawai-OSS-spaced.zip *
curl -sL https://git.io/file-transfer | sh
./transfer  nekokawai-OSS-spaced.zip
}

compile
zupload
