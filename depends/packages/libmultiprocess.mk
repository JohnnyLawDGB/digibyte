package=libmultiprocess
$(package)_version=$(native_$(package)_version)
$(package)_download_path=$(native_$(package)_download_path)
$(package)_file_name=$(native_$(package)_file_name)
$(package)_sha256_hash=$(native_$(package)_sha256_hash)
<<<<<<< HEAD
$(package)_dependencies=native_$(package) boost capnp

define $(package)_config_cmds
  $($(package)_cmake)
=======
$(package)_dependencies=native_$(package) capnp
ifneq ($(host),$(build))
$(package)_dependencies += native_capnp
endif

define $(package)_set_vars :=
ifneq ($(host),$(build))
$(package)_config_opts := -DCAPNP_EXECUTABLE="$$(native_capnp_prefixbin)/capnp"
$(package)_config_opts += -DCAPNPC_CXX_EXECUTABLE="$$(native_capnp_prefixbin)/capnpc-c++"
endif
endef

define $(package)_config_cmds
  $($(package)_cmake) .
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
endef

define $(package)_build_cmds
  $(MAKE)
endef

define $(package)_stage_cmds
<<<<<<< HEAD
  $(MAKE) DESTDIR=$($(package)_staging_dir) install
=======
  $(MAKE) DESTDIR=$($(package)_staging_dir) install-lib
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
endef
