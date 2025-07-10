package=native_libtapi
<<<<<<< HEAD
$(package)_version=664b8414f89612f2dfd35a9b679c345aa5389026
$(package)_download_path=https://github.com/tpoechtrager/apple-libtapi/archive
$(package)_download_file=$($(package)_version).tar.gz
$(package)_file_name=$($(package)_version).tar.gz
$(package)_sha256_hash=62e419c12d1c9fad67cc1cd523132bc00db050998337c734c15bc8d73cc02b61
=======
$(package)_version=eb33a59f2e30ff9724dc1ea8bee8b5229b0557c9
$(package)_download_path=https://github.com/tpoechtrager/apple-libtapi/archive
$(package)_file_name=$($(package)_version).tar.gz
$(package)_sha256_hash=d4d46c64622f13d6938cecf989046d9561011bb59e8ee835f8f39825d67f578f
$(package)_patches=disable_zlib.patch
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion

ifeq ($(strip $(FORCE_USE_SYSTEM_CLANG)),)
$(package)_dependencies=native_clang
endif

<<<<<<< HEAD
=======
define $(package)_preprocess_cmds
  patch -p1 < $($(package)_patch_dir)/disable_zlib.patch
endef

>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
define $(package)_build_cmds
  CC=$(clang_prog) CXX=$(clangxx_prog) INSTALLPREFIX=$($(package)_staging_prefix_dir) ./build.sh
endef

define $(package)_stage_cmds
<<<<<<< HEAD
  ./install.sh && \
  mkdir -p $($(package)_staging_prefix_dir)/include/llvm-c && \
  cp src/llvm/include/llvm-c/lto.h $($(package)_staging_prefix_dir)/include/llvm-c
=======
  ./install.sh
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
endef
