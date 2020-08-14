# WARNING: Likely need to specify compiler to use, e.g. `CC=g++ make release`, if not want to use cc


BUILD_DIR = ./build


# Git: Calculate the current build version, which is one higher than the current tag, unless the current commit is tagged
V_commits := $(shell git rev-list `git rev-list --tags --no-walk --max-count=1`..HEAD --count)
# Number of commits since last version declared
TAG  := $(shell git describe --abbrev=0 --tags)
ifeq ($(V_commits), 0)
V    := $(TAG)
else
TAG1 := $(shell echo "$(TAG)" | sed 's/[0-9]*$$//g')
TAG2 := $(shell echo "$(TAG)" | sed 's/.*[^0-9]//g')
#V    := $(TAG1)$(shell echo $$(($(TAG2)+1))).$(V_commits)
V    := $(TAG).$(V_commits)
endif


EXEPATH = $(BUILD_DIR)/bpcs_$(V)
EXTPATH = $(BUILD_DIR)/bpcs-e_$(V)
EXVPATH = $(BUILD_DIR)/bpcs-v_$(V)
FMEPATH = $(BUILD_DIR)/bpcs-fmt_$(V)
FMTPATH = $(BUILD_DIR)/bpcs-fmt-e_$(V)
FMVPATH = $(BUILD_DIR)/bpcs-fmt-v_$(V)


LINKER_OPTS = --gc-sections --build-id=none

DEBUGFLAGS = -DDEBUG -DTESTS -rdynamic -DEMBEDDOR -g
TINYFLAGS = -ffunction-sections -fdata-sections -fno-rtti -fvisibility=hidden -fvisibility-inlines-hidden
NODEBUGFLAGS = -DNDEBUG -fno-exceptions -DARGS_NOEXCEPT



LINKER_OPTS += -lsodium


LIBRARY_PATHS=
INCLUDES= include $(HOME)/include
OBJ_FILES=



ifneq ($(OPENCV_DIR), )
INCLUDES += $(OPENCV_DIR)/include/opencv4
LIBRARY_PATHS += $(OPENCV_DIR)/lib
LINKER_OPTS += -lopencv_core -rpath=$(OPENCV_DIR)/lib
else
LINKER_OPTS += -lopencv_core
endif

ifneq ($(LIBPNG_DIR), )
INCLUDES += $(LIBPNG_DIR)/include
LIBRARY_PATHS += $(LIBPNG_DIR)/lib
LINKER_OPTS += -lpng -rpath=$(LIBPNG_DIR)/lib
else
LINKER_OPTS += -lpng
endif

ifneq ($(USE_CUDA), )
#LIBRARY_PATHS += $(CUDA_DIR)/include
LINKER_OPTS += -lcuda -lcudart
GPU_MAT_SRC = gpu_mat_ops.o
BPCS_SRC=bpcs_cuda.cpp
else
GPU_MAT_SRC = 
BPCS_SRC=bpcs.cpp
endif






RELEASEFLAGS_ = -Ofast -s -frename-registers $(NODEBUGFLAGS) $(TINYFLAGS) -march=native -fgcse-las -fno-stack-protector -funsafe-loop-optimizations -Wunsafe-loop-optimizations
RELEASEFLAGS = $(RELEASEFLAGS_) -flto
# nvcc is not capable of LTO. g++ forgets what -Wl,... means when it is passed through by nvcc -compiler-optioins




LINKER_FLAGS = $(foreach opt, $(LINKER_OPTS), -Wl,$(opt))

LIBRARY_PARAMS = $(foreach d, $(LIBRARY_PATHS), -L$d)
INCLUDE_PARAMS = $(foreach d, $(INCLUDES), -I$d)
STD_PARAMS = $(OBJ_FILES) $(LIBRARY_PARAMS) $(INCLUDE_PARAMS) $(LINKER_FLAGS)


STRIP_ARGS_VERNEEN_RECORD =  -R .gnu.version_r
# Causes it to fail to run on my Linux machine
STRIP_ARGS = --strip-unneeded -R .comment -R .note -R .gnu.version -R .note.ABI-tag
# In ascending order of dubiousness.
# .note.ABI-tag stores info on which systems compatible with. May prevent executable from running on FreeBSD or OS with Linux emulation support
# .rodata stores OpenCV error messages, "basic_string::_M_construct null not valid.total() == 0 || data != NULL./usr/local/include/opencv2/core/mat.inl.hpp.Step must be a multiple of esz1...png.list.size() != 0.[NULL].stof.r.Mat.Mat."

CPPFLAGS_ = -Wall


CCs = g++-6 g++-7 g++-8 clang++


ifeq ($(CC), cc)
CC = g++
RELEASEFLAGS += -fira-loop-pressure
CPPFLAGS_ += -fstack-usage
endif

CPPFLAGS = $(CPPFLAGS_) $(BPCS_SRC)


CUDA_COMPILER_OPTS := $(foreach opt, $(CPPFLAGS_), --compiler-options $(opt)) $(foreach opt, $(LINKER_OPTS), --linker-options $(opt))
CUDA_COMPILER_RELEASE_OPTS := $(CUDA_COMPILER_OPTS) $(foreach opt, $(RELEASEFLAGS_), --compiler-options $(opt))
CUDA_COMPILER_DEBUG_OPTS := $(CUDA_COMPILER_OPTS) $(foreach opt, $(DEBUGFLAGS), --compiler-options $(opt))


define DOC
	pandoc -s -t man doc/$(1).md -o doc/$(1)
	
endef

docs:
	$(foreach basename, bpcs.1 bpcs.3 bpcs-v.1 bpcs-fmt.1, $(call DOC,$(basename)))
	cat README.md | sed -r 's~\[[^]]+\]\(doc/([a-z-]+)\.md\)\.?~`\1(1)`~g' | pandoc -s -t man -o doc/bpcs-doc.1


debug-main:
	$(CC) $(CPPFLAGS) -o $(EXVPATH) $(STD_PARAMS) $(DEBUGFLAGS)

debug-fmt:
	$(CC) $(CPPFLAGS_) fmt.cpp -o $(FMVPATH) $(STD_PARAMS) $(DEBUGFLAGS)

debug:
	$(CC) $(CPPFLAGS) -o $(EXVPATH) $(STD_PARAMS) $(DEBUGFLAGS)
	$(CC) $(CPPFLAGS_) fmt.cpp -o $(FMVPATH) $(STD_PARAMS) $(DEBUGFLAGS)


define RELEASE
	$(1) $(CPPFLAGS_) $(2) $(3) -o $(4)_  $(STD_PARAMS) $(RELEASEFLAGS) $(5)
	strip $(STRIP_ARGS) $(4)_
	
endef


define RELEASE_BPCS
	$(call RELEASE,$(1),$(GPU_MAT_SRC),$(BPCS_SRC),$(EXTPATH),)
	$(call RELEASE,$(1),$(GPU_MAT_SRC),$(BPCS_SRC),$(EXEPATH), -DEMBEDDOR)
	
endef


define RELEASE_FMT
	$(call RELEASE,$(1),$(GPU_MAT_SRC),fmt.cpp,$(FMTPATH),)
	$(call RELEASE,$(1),$(GPU_MAT_SRC),fmt.cpp,$(FMEPATH), -DEMBEDDOR)
	
endef


release:
	$(call RELEASE_BPCS,$(CC))
	$(call RELEASE_FMT,$(CC))

release-main:
	$(call RELEASE_BPCS,$(CC))

debug-cuda:
	nvcc -O0 $(CUDA_COMPILER_DEBUG_OPTS) -ccbin=g++ bpcs_cuda.cu -o build/bpcs_cuda $(LIBRARY_PARAMS) $(INCLUDE_PARAMS) $(LDLIBS)

release-cuda:
	nvcc $(CUDA_COMPILER_RELEASE_OPTS) -ccbin=g++ bpcs_cuda.cu -o build/bpcs_cuda $(LIBRARY_PARAMS) $(INCLUDE_PARAMS) $(LDLIBS)

release-fmt:
	$(call RELEASE_FMT,$(CC))


profile:
	$(call RELEASE,$(CC),$(GPU_MAT_SRC),$(BPCS_SRC),$(EXEPATH)_prof, -DEMBEDDOR -fprofile-generate)

release-profiled:
	$(CC) $(CPPFLAGS) -o $(EXEPATH)      $(STD_PARAMS) $(RELEASEFLAGS) -fprofile-use -fprofile-correction
	strip $(STRIP_ARGS) $(EXEPATH)



release-archive:
	REPO_TMP_FP="/tmp/bpcs.release-archive"
	
	git rev-list --tags --no-walk --oneline >> "/tmp/git-hist.txt"
	cd /tmp
	git clone "$(PWD)" "$(REPO_TMP_FP)"
	
	while read line; do
		SHA1="$${line%% *}"
		git reset --hard "$$SHA1"
		make --directory "$(REPO_TMP_FP)" release
	done < "/tmp/git-hist.txt"




compare:
	$(foreach cc, $(CCs), $(call RELEASE_BPCS,$(cc)))
	$(foreach cc, $(CCs), $(call RELEASE_FMT,$(cc)))




define MINSRC
	$(CC) $(CPPFLAGS) -o /tmp/$(1)_macros.cpp -E $(2)
	cat $(BPCS_SRC) | egrep '^(#include|namespace .*\{(\n +#include .*)+\n\})' > $(1)_macros_
	cat $(BPCS_SRC) | grep '#define ' >> $(1)_macros_
	cat -s /tmp/$(1)_macros.cpp | egrep -A 99999 'typedef cv::Matx<uchar, 9, 9> Matx99uc;' | egrep -v '^# [0-9]+ "bpcs[.]cpp"' | sed 's/[(][(][(]0[)] & [(][(]1 << 3[)] - 1[)][)] + [(][(][(]1[)]-1[)] << 3[)][)]/CV_8UC1/g' >> $(1)_macros_
	mv $(1)_macros_ /tmp/$(1)_macros.cpp
	sed -i -r 's/\n *CV_8UC1\n *([^ ])/CV_8UC1\1/' /tmp/$(1)_macros.cpp # Regex works in Kate, not sed...
	sed -i -r 's/\{\n\n/{\n/' /tmp/$(1)_macros.cpp # same
	perl -p0e 's/\n *CV_8UC1\n *([^ ])/CV_8UC1\1/g' -i /tmp/$(1)_macros.cpp
	perl -p0e 's/\{\n *\n/{\n/g' -i /tmp/$(1)_macros.cpp
	perl -p0e 's/\n(\n *\})/\1/g' -i /tmp/$(1)_macros.cpp
	perl -p0e 's/\)\n\n/)\n/g' -i /tmp/$(1)_macros.cpp
	perl -p0e 's/(==) \n *([^ ]+)\n *\)/\1 \2)/g' -i /tmp/$(1)_macros.cpp
	perl -p0e 's/(^[v]|^v[^o]|^vo[^i]|^voi[^d]|^void[^ ].*)\{([^;}]+;)(\n *)\}\n/\1\2\3/g' -i /tmp/$(1)_macros.cpp # Remove brackets from multiline scopes
	perl -p0e 's/\n\n+/\n/g' -i /tmp/$(1)_macros.cpp # Remove excess lines
	perl -p0e 's/\(\n    (.+)\n\)/(\1)/g' -i /tmp/$(1)_macros.cpp # Move functions with single parameter to single line
	perl -p0e 's/\n *(\n *[}])/\1/g' -i /tmp/$(1)_macros.cpp # Remove lines before }
	perl -p0e 's~/\*([^/]\*|/[^\*])*\*/~~g' -i /tmp/$(1)_macros.cpp # Remove comments
	perl -p0e 's~//[^\n]*~~g' -i /tmp/$(1)_macros.cpp # Remove comments
	perl -p0e 's/case \n *([0-9]+)\n *:\n/case \1:/g' -i /tmp/$(1)_macros.cpp # Remove excess lines created when compiler expands definitions such as CV_8U
	perl -p0e 's/,\n *([^ \n])/, \1/g' -i /tmp/$(1)_macros.cpp # Remove excess lines created when macros change number of elements in a list
	perl -p0e 's/(^[v]|^v[^o]|^vo[^i]|^voi[^d]|^void[^ ].*)\{([^;}]+;)(\n *)\}\n/\1\2\3/g' -i /tmp/$(1)_macros.cpp # Remove brackets from multiline scopes (2nd pass)
	perl -p0e 's/([^;{}])\n {8,}([^ ])/\1\2/g' -i /tmp/$(1)_macros.cpp
	perl -p0e 's/(if \([^)]+\))([^ \n;{}])/\1 \2/g' -i /tmp/$(1)_macros.cpp
	
	mv /tmp/$(1)_macros.cpp $(1)_macros.cpp
	
endef

minsrc:
	$(call MINSRC,$(EXEPATH),-DEMBEDDOR)
	$(call MINSRC,$(EXTPATH),)
