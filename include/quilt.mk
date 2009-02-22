# 
# Copyright (C) 2007-2009 OpenWrt.org
#
# This is free software, licensed under the GNU General Public License v2.
# See /LICENSE for more information.
#

ifneq ($(__quilt_inc),1)
__quilt_inc:=1

ifeq ($(TARGET_BUILD),1)
  PKG_BUILD_DIR:=$(LINUX_DIR)
endif
PATCH_DIR?=./patches
FILES_DIR?=./files
HOST_PATCH_DIR?=$(PATCH_DIR)
HOST_FILES_DIR?=$(FILES_DIR)

ifeq ($(MAKECMDGOALS),refresh)
  override QUILT=1
endif

QUILT_CMD:=quilt --quiltrc=-

define filter_series
sed -e s,\\\#.*,, $(1) | grep -E \[a-zA-Z0-9\]
endef

define PatchDir/Quilt
	@if [ -s "$(2)/series" ]; then \
		mkdir -p "$(1)/patches/$(3)"; \
		cp "$(2)/series" "$(1)/patches/$(3)"; \
	fi
	@for patch in $$$$( (cd "$(2)" && if [ -f series ]; then $(call filter_series,series); else ls; fi; ) 2>/dev/null ); do ( \
		cp "$(2)/$$$$patch" "$(1)"; \
		cd "$(1)"; \
		$(QUILT_CMD) import -P$(3)$$$$patch -p 1 "$$$$patch"; \
		$(QUILT_CMD) push -f >/dev/null 2>/dev/null; \
		rm -f "$$$$patch"; \
	); done
	$(if $(3),@echo $(3) >> "$(1)/patches/.subdirs")
endef

define PatchDir/Default
	@if [ -d "$(2)" -a "$$$$(ls $(2) | wc -l)" -gt 0 ]; then \
		if [ -s "$(2)/series" ]; then \
			$(call filter_series,$(2)/series) | xargs -n1 \
				$(PATCH) "$(1)" "$(2)"; \
		else \
			$(PATCH) "$(1)" "$(2)"; \
		fi; \
	fi
endef

define PatchDir
$(call PatchDir/$(if $(strip $(QUILT)),Quilt,Default),$(strip $(1)),$(strip $(2)),$(strip $(3)))
endef

ifneq ($(PKG_BUILD_DIR),)
  QUILT?=$(strip $(shell test -f $(PKG_BUILD_DIR)/.quilt_used && echo y))
  ifneq ($(QUILT),)
    STAMP_PATCHED:=$(PKG_BUILD_DIR)/.quilt_patched
    STAMP_CHECKED:=$(PKG_BUILD_DIR)/.quilt_checked
    override CONFIG_AUTOREBUILD=
    prepare: $(STAMP_PATCHED)
    quilt-check: $(STAMP_CHECKED)
  endif
endif

ifneq ($(HOST_BUILD_DIR),)
  HOST_QUILT?=$(strip $(shell test -f $(if $(PKG_BUILD_DIR),$(PKG_BUILD_DIR),$(HOST_BUILD_DIR))/.quilt_used && echo y))
  ifneq ($(HOST_QUILT),)
    HOST_STAMP_PATCHED:=$(HOST_BUILD_DIR)/.quilt_patched
    HOST_STAMP_CHECKED:=$(HOST_BUILD_DIR)/.quilt_checked
    override CONFIG_AUTOREBUILD=
    host-prepare: $(HOST_STAMP_PATCHED)
    host-quilt-check: $(HOST_STAMP_CHECKED)
  endif
endif

define Host/Patch/Default
	$(if $(QUILT),rm -rf $(HOST_BUILD_DIR)/patches; mkdir -p $(HOST_BUILD_DIR)/patches)
	$(call PatchDir,$(HOST_BUILD_DIR),$(HOST_PATCH_DIR),)
	$(if $(QUILT),touch $(HOST_BUILD_DIR)/.quilt_used)
endef

define Build/Patch/Default
	$(if $(QUILT),rm -rf $(PKG_BUILD_DIR)/patches; mkdir -p $(PKG_BUILD_DIR)/patches)
	$(call PatchDir,$(PKG_BUILD_DIR),$(PATCH_DIR),)
	$(if $(QUILT),touch $(PKG_BUILD_DIR)/.quilt_used)
endef

kernel_files=$(foreach fdir,$(GENERIC_FILES_DIR) $(FILES_DIR),$(fdir)/.)
define Kernel/Patch/Default
	rm -rf $(PKG_BUILD_DIR)/patches; mkdir -p $(PKG_BUILD_DIR)/patches
	$(if $(kernel_files),$(CP) $(kernel_files) $(LINUX_DIR)/)
	find $(LINUX_DIR)/ -name \*.rej -or -name \*.orig | $(XARGS) rm -f
	$(call PatchDir,$(PKG_BUILD_DIR),$(GENERIC_PATCH_DIR),generic/)
	$(call PatchDir,$(PKG_BUILD_DIR),$(PATCH_DIR),platform/)
endef

define Quilt/RefreshDir
	mkdir -p $(2)
	-rm -f $(2)/* 2>/dev/null >/dev/null
	@( \
		for patch in $$$$($(if $(3),grep "^$(3)",cat) $(PKG_BUILD_DIR)/patches/series | awk '{print $$$$1}'); do \
			$(CP) -v "$(PKG_BUILD_DIR)/patches/$$$$patch" $(2); \
		done; \
	)
endef

define Quilt/Refresh/Host
	$(call Quilt/RefreshDir,$(HOST_BUILD_DIR),$(PATCH_DIR))
endef

define Quilt/Refresh/Package
	$(call Quilt/RefreshDir,$(PKG_BUILD_DIR),$(PATCH_DIR))
endef

define Quilt/Refresh/Kernel
	@[ -z "$$(grep -v '^generic/' $(PKG_BUILD_DIR)/patches/series | grep -v '^platform/')" ] || { \
		echo "All kernel patches must start with either generic/ or platform/"; \
		false; \
	}
	$(call Quilt/RefreshDir,$(PKG_BUILD_DIR),$(GENERIC_PATCH_DIR),generic/)
	$(call Quilt/RefreshDir,$(PKG_BUILD_DIR),$(PATCH_DIR),platform/)
endef

define Quilt/Template
  $($(2)STAMP_PATCHED): $($(2)STAMP_PREPARED)
	@( \
		cd $(1)/patches; \
		$(QUILT_CMD) pop -a -f >/dev/null 2>/dev/null; \
		if [ -s ".subdirs" ]; then \
			rm -f series; \
			for file in $$$$(cat .subdirs); do \
				if [ -f $$$$file/series ]; then \
					echo "Converting $$file/series"; \
					$$(call filter_series,$$$$file/series) | awk -v file="$$$$file/" '$$$$0 !~ /^#/ { print file $$$$0 }' | sed -e s,//,/,g >> series; \
				else \
					echo "Sorting patches in $$$$file"; \
					find $$$$file/* -type f \! -name series | sed -e s,//,/,g | sort >> series; \
				fi; \
			done; \
		else \
			find * -type f \! -name series | sort > series; \
		fi; \
	)
	touch "$$@"

  $($(2)STAMP_CONFIGURED): $($(2)STAMP_CHECKED) FORCE
  $($(2)STAMP_CHECKED): $($(2)STAMP_PATCHED)
	if [ -s "$(1)/patches/series" ]; then \
		(cd "$(1)"; \
			if $(QUILT_CMD) next >/dev/null 2>&1; then \
				$(QUILT_CMD) push -a; \
			else \
				$(QUILT_CMD) top >/dev/null 2>&1; \
			fi \
		); \
	fi
	touch "$$@"

  $(3)quilt-check: $($(2)STAMP_PREPARED) FORCE
	@[ -f "$(1)/.quilt_used" ] || { \
		echo "The source directory was not unpacked using quilt. Please rebuild with QUILT=1"; \
		false; \
	}
	@[ -f "$(1)/patches/series" ] || { \
		echo "The source directory contains no quilt patches."; \
		false; \
	}
	@[ -n "$$$$(ls $(1)/patches/series)" -o "$$$$(cat $(1)/patches/series | md5sum)" = "$$(sort $(1)/patches/series | md5sum)" ] || { \
		echo "The patches are not sorted in the right order. Please fix."; \
		false; \
	}

  $(3)refresh: $(3)quilt-check
	@cd "$(1)"; $(QUILT_CMD) pop -a -f >/dev/null 2>/dev/null
	@cd "$(1)"; while $(QUILT_CMD) next 2>/dev/null >/dev/null && $(QUILT_CMD) push; do \
		QUILT_DIFF_OPTS="-p" $(QUILT_CMD) refresh -p ab --no-index --no-timestamps; \
	done; ! $(QUILT_CMD) next 2>/dev/null >/dev/null
	$(Quilt/Refresh/$(4))
	
  $(3)update: $(3)quilt-check
	$(Quilt/Refresh/$(4))
endef

Build/Quilt=$(call Quilt/Template,$(PKG_BUILD_DIR),,,$(if $(TARGET_BUILD),Kernel,Package))
Host/Quilt=$(call Quilt/Template,$(HOST_BUILD_DIR),HOST_,host-,Host)

endif
