package=native_libmultiprocess
<<<<<<< HEAD
$(package)_version=d576d975debdc9090bd2582f83f49c76c0061698
$(package)_download_path=https://github.com/chaincodelabs/libmultiprocess/archive
$(package)_file_name=$($(package)_version).tar.gz
$(package)_sha256_hash=9f8b055c8bba755dc32fe799b67c20b91e7b13e67cadafbc54c0f1def057a370
$(package)_dependencies=native_capnp

define $(package)_config_cmds
  $($(package)_cmake)
=======
$(package)_version=1af83d15239ccfa7e47b8764029320953dd7fdf1
$(package)_download_path=https://github.com/chaincodelabs/libmultiprocess/archive
$(package)_file_name=$($(package)_version).tar.gz
$(package)_sha256_hash=e5587d3feedc7f8473f178a89b94163a11076629825d664964799bbbd5844da5
$(package)_dependencies=native_capnp

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
  $(MAKE) DESTDIR=$($(package)_staging_dir) install-bin
>>>>>>> bitcoin-v26-2-converted/digibyte-v26.2-naming-conversion
endef
