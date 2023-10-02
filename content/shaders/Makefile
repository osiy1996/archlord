# Make sure that platform variable is set.
ifndef PLATFORM
	ifdef platform
			# `platform` can be substituted for `PLATFORM`.
			PLATFORM = $(platform)
		else
            $(info Platform isn't set, defaulting to windows.)
			PLATFORM = windows
	endif
endif

ifeq ($(PLATFORM), windows)
    RM = del /Q
    FixPath = $(subst /,\,$1)
endif

COMPILER = $(call FixPath,../../tools/msvc/shaderc)
TARGETS = spirv dx9 dx11
VSOURCE = $(wildcard *.vs.sc)
VBIN = $(foreach src,$(subst .vs.sc,,$(VSOURCE)),$(src).vs.bin)
FSOURCE = $(wildcard *.fs.sc)
FBIN = $(foreach src,$(subst .fs.sc,,$(FSOURCE)),$(src).fs.bin)

VPROFILE_spirv = spirv
FPROFILE_spirv = spirv
VPROFILE_dx9 = vs_3_0
FPROFILE_dx9 = ps_3_0
VPROFILE_dx11 = vs_5_0
FPROFILE_dx11 = ps_5_0

BuildTargets = $(foreach target,$(TARGETS),$(target)/$1)

all: $(foreach bin,$(VBIN),$(call BuildTargets,$(bin))) \
	$(foreach bin,$(FBIN),$(call BuildTargets,$(bin)))

%.vs.bin: ../%.vs.sc
	$(COMPILER) --platform $(PLATFORM) \
		-p $(VPROFILE_$(subst /,,$(dir $@))) --type vertex \
		-o $@ -f $(notdir $(subst .vs.bin,.vs.sc,$@))

%.fs.bin: ../%.fs.sc
	$(COMPILER) --platform $(PLATFORM) \
		-p $(FPROFILE_$(subst /,,$(dir $@))) --type fragment \
		-o $@ -f $(notdir $(subst .fs.bin,.fs.sc,$@))

clean:
	$(RM) $(foreach dir,$(TARGETS),$(call FixPath,$(dir)/*))

rebuild: clean all

.PHONY: clean
