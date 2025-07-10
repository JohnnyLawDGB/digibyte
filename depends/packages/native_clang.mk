package=native_clang
<<<<<<< HEAD
$(package)_version=10.0.1
$(package)_download_path=https://github.com/llvm/llvm-project/releases/download/llvmorg-$($(package)_version)
ifneq (,$(findstring aarch64,$(BUILD)))
$(package)_download_file=clang+llvm-$($(package)_version)-aarch64-linux-gnu.tar.xz
$(package)_file_name=clang+llvm-$($(package)_version)-aarch64-linux-gnu.tar.xz
$(package)_sha256_hash=90dc69a4758ca15cd0ffa45d07fbf5bf4309d47d2c7745a9f0735ecffde9c31f
else
$(package)_download_file=clang+llvm-$($(package)_version)-x86_64-linux-gnu-ubuntu-16.04.tar.xz
$(package)_file_name=clang+llvm-$($(package)_version)-x86_64-linux-gnu-ubuntu-16.04.tar.xz
$(package)_sha256_hash=48b83ef827ac2c213d5b64f5ad7ed082c8bcb712b46644e0dc5045c6f462c231
endif

define $(package)_preprocess_cmds
  rm -f $($(package)_extract_dir)/lib/libc++abi.so*
endef

define $(package)_stage_cmds
  mkdir -p $($(package)_staging_prefix_dir)/lib/clang/$($(package)_version)/include && \
  mkdir -p $($(package)_staging_prefix_dir)/bin && \
  mkdir -p $($(package)_staging_prefix_dir)/include && \
=======
$(package)_version=15.0.6
$(package)_download_path=https://github.com/llvm/llvm-project/releases/download/llvmorg-$($(package)_version)
ifneq (,$(findstring aarch64,$(BUILD)))
$(package)_file_name=clang+llvm-$($(package)_version)-aarch64-linux-gnu.tar.xz
$(package)_sha256_hash=8ca4d68cf103da8331ca3f35fe23d940c1b78fb7f0d4763c1c059e352f5d1bec
else
$(package)_file_name=clang+llvm-$($(package)_version)-x86_64-linux-gnu-ubuntu-18.04.tar.xz
$(package)_sha256_hash=38bc7f5563642e73e69ac5626724e206d6d539fbef653541b34cae0ba9c3f036
endif

define $(package)_stage_cmds
  mkdir -p $($(package)_staging_prefix_dir)/lib/clang/$($(package)_version)/include && \
  mkdir -p $($(package)_staging_prefix_dir)/bin && \
  mkdir -p $($(package)_staging_prefix_dir)/include/llvm-c && \
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
  cp bin/clang $($(package)_staging_prefix_dir)/bin/ && \
  cp -P bin/clang++ $($(package)_staging_prefix_dir)/bin/ && \
  cp bin/dsymutil $($(package)_staging_prefix_dir)/bin/$(host)-dsymutil && \
  cp bin/llvm-config $($(package)_staging_prefix_dir)/bin/ && \
<<<<<<< HEAD
  cp lib/libLTO.so $($(package)_staging_prefix_dir)/lib/ && \
  cp -rf lib/clang/$($(package)_version)/include/* $($(package)_staging_prefix_dir)/lib/clang/$($(package)_version)/include/
endef

define $(package)_postprocess_cmds
  rmdir include
=======
  cp include/llvm-c/ExternC.h $($(package)_staging_prefix_dir)/include/llvm-c && \
  cp include/llvm-c/lto.h $($(package)_staging_prefix_dir)/include/llvm-c && \
  cp lib/libLTO.so $($(package)_staging_prefix_dir)/lib/ && \
  cp -r lib/clang/$($(package)_version)/include/* $($(package)_staging_prefix_dir)/lib/clang/$($(package)_version)/include/
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
endef
