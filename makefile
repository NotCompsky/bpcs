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
FMTPATH = $(BUILD_DIR)/bpcs-fmt_$(V)
FMEPATH = $(BUILD_DIR)/bpcs-fmt-e_$(V)
FMVPATH = $(BUILD_DIR)/bpcs-fmt-v_$(V)


DEBUGFLAGS = -DDEBUG -DTESTS
TINYFLAGS = -ffunction-sections -fdata-sections -Wl,--gc-sections -Wl,--build-id=none -fno-rtti -fvisibility=hidden -fvisibility-inlines-hidden
NODEBUGFLAGS = -DNDEBUG -fno-exceptions -DARGS_NOEXCEPT
RELEASEFLAGS = -Ofast -s -frename-registers $(NODEBUGFLAGS) $(TINYFLAGS) -march=native -flto -fgcse-las -fno-stack-protector -funsafe-loop-optimizations -Wunsafe-loop-optimizations
LDLIBS = -lsodium


LIBRARY_PATHS=
INCLUDES= include $(HOME)/include
OBJ_FILES=

ifneq ($(OPENCV_DIR), "")
INCLUDES += $(OPENCV_DIR)/include/opencv4
LIBRARY_PATHS += $(OPENCV_DIR)/lib
LDLIBS += -lopencv_core -Wl,-rpath=$(OPENCV_DIR)/lib
else
LDLIBS += -lopencv_core
endif
ifneq ($(LIBPNG_DIR), "")
INCLUDES += $(LIBPNG_DIR)/include
LIBRARY_PATHS += $(LIBPNG_DIR)/lib
LDLIBS += -lpng -Wl,-rpath=$(LIBPNG_DIR)/lib
else
LDLIBS += -lpng
endif


LIBRARY_PARAMS = $(foreach d, $(LIBRARY_PATHS), -L$d)
INCLUDE_PARAMS = $(foreach d, $(INCLUDES), -I$d)
STD_PARAMS = $(OBJ_FILES) $(LIBRARY_PARAMS) $(INCLUDE_PARAMS) $(LDLIBS)


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

CPPFLAGS = $(CPPFLAGS_) bpcs.cpp



docs:
	pandoc -s -t man doc/bpcs.md -o doc/bpcs.1
	pandoc -s -t man doc/bpcs-v.md -o doc/bpcs-v.1
	pandoc -s -t man doc/bpcs-fmt.md -o doc/bpcs-fmt.1
	cat README.md | sed -r 's~\[[^]]+\]\(doc/([a-z-]+)\.md\)\.?~`\1(1)`~g' | pandoc -s -t man -o doc/bpcs-doc.1


debug:
	$(CC) $(CPPFLAGS) -o $(EXVPATH) $(STD_PARAMS) $(DEBUGFLAGS) -DEMBEDDOR -g
	$(CC) $(CPPFLAGS_) fmt.cpp -o $(FMVPATH) $(STD_PARAMS) $(DEBUGFLAGS) -DEMBEDDOR -g


define RELEASE
	$(1) $(CPPFLAGS) -o $(EXEPATH)_$(1)_   $(STD_PARAMS) $(RELEASEFLAGS) -DEMBEDDOR
	$(1) $(CPPFLAGS) -o $(EXTPATH)_$(1)_   $(STD_PARAMS) $(RELEASEFLAGS)
	strip $(STRIP_ARGS) $(EXEPATH)_$(1)_
	strip $(STRIP_ARGS) $(EXTPATH)_$(1)_
	
endef


release:
	$(call RELEASE,$(CC))

fmt:
	$(CC) $(CPPFLAGS_) fmt.cpp -o $(FMEPATH)_  $(STD_PARAMS) $(RELEASEFLAGS)
	$(CC) $(CPPFLAGS_) fmt.cpp -o $(FMTPATH)_  $(STD_PARAMS) $(RELEASEFLAGS) -DEMBEDDOR
	strip $(STRIP_ARGS) $(FMEPATH)_
	strip $(STRIP_ARGS) $(FMTPATH)_


profile:
	$(CC) $(CPPFLAGS) -o $(EXEPATH)_prof $(STD_PARAMS) $(RELEASEFLAGS) -fprofile-generate

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
	$(foreach cc, $(CCs), $(call RELEASE,$(cc)))




define MINSRC
	$(CC) $(CPPFLAGS) -o $(1)_macros.cpp -E $(2)
	cat bpcs.cpp | egrep '^(#include|namespace .*\{(\n +#include .*)+\n\})' > $(1)_macros_
	cat bpcs.cpp | grep '#define ' >> $(1)_macros_
	cat -s $(1)_macros.cpp | egrep -A 99999 'typedef cv::Matx<uchar, 8, 8> Matx88uc;' | egrep -v '^# [0-9]+ "bpcs[.]cpp"' | sed 's/[(][(][(]0[)] & [(][(]1 << 3[)] - 1[)][)] + [(][(][(]1[)]-1[)] << 3[)][)]/CV_8UC1/g' >> $(1)_macros_
	mv $(1)_macros_ $(1)_macros.cpp
	sed -i -r 's/\n *CV_8UC1\n *([^ ])/CV_8UC1\1/' $(1)_macros.cpp # Regex works in Kate, not sed...
	sed -i -r 's/\{\n\n/{\n/' $(1)_macros.cpp # same
	perl -p0e 's/\n *CV_8UC1\n *([^ ])/CV_8UC1\1/g' -i $(1)_macros.cpp
	perl -p0e 's/\{\n *\n/{\n/g' -i $(1)_macros.cpp
	perl -p0e 's/\n(\n *\})/\1/g' -i $(1)_macros.cpp
	perl -p0e 's/\)\n\n/)\n/g' -i $(1)_macros.cpp
	perl -p0e 's/(==) \n *([^ ]+)\n *\)/\1 \2)/g' -i $(1)_macros.cpp
	perl -p0e 's/(^[v]|^v[^o]|^vo[^i]|^voi[^d]|^void[^ ].*)\{([^;}]+;)(\n *)\}\n/\1\2\3/g' -i $(1)_macros.cpp # Remove brackets from multiline scopes
	perl -p0e 's/\n\n+/\n/g' -i $(1)_macros.cpp # Remove excess lines
	perl -p0e 's/\(\n    (.+)\n\)/(\1)/g' -i $(1)_macros.cpp # Move functions with single parameter to single line
	perl -p0e 's/\n *(\n *[}])/\1/g' -i $(1)_macros.cpp # Remove lines before }
	perl -p0e 's~/\*([^/]\*|/[^\*])*\*/~~g' -i $(1)_macros.cpp # Remove comments
	perl -p0e 's~//[^\n]*~~g' -i $(1)_macros.cpp # Remove comments
	perl -p0e 's/case \n *([0-9]+)\n *:\n/case \1:/g' -i $(1)_macros.cpp # Remove excess lines created when compiler expands definitions such as CV_8U
	perl -p0e 's/,\n *([^ \n])/, \1/g' -i $(1)_macros.cpp # Remove excess lines created when macros change number of elements in a list
	perl -p0e 's/(^[v]|^v[^o]|^vo[^i]|^voi[^d]|^void[^ ].*)\{([^;}]+;)(\n *)\}\n/\1\2\3/g' -i $(1)_macros.cpp # Remove brackets from multiline scopes (2nd pass)
	
endef

minsrc:
	$(call MINSRC,$(EXEPATH),-DEMBEDDOR)
	$(call MINSRC,$(EXTPATH),)
