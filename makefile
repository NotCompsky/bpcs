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


DEBUGFLAGS = -DDEBUG -DTESTS
TINYFLAGS = -ffunction-sections -fdata-sections -Wl,--gc-sections -Wl,--build-id=none -fno-rtti -fvisibility=hidden -fvisibility-inlines-hidden
NODEBUGFLAGS = -DNDEBUG -fno-exceptions -DARGS_NOEXCEPT
RELEASEFLAGS = -Ofast -s -frename-registers $(NODEBUGFLAGS) $(TINYFLAGS) -march=native -flto -fgcse-las -funsafe-loop-optimizations -Wunsafe-loop-optimizations
LDLIBS = -lsodium


LIBRARY_PATHS = ""
INCLUDES = include $(HOME)/include

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
STD_PARAMS = $(LIBRARY_PARAMS) $(INCLUDE_PARAMS) $(LDLIBS)


STRIP_ARGS_VERNEEN_RECORD =  -R .gnu.version_r
# Causes it to fail to run on my Linux machine
STRIP_ARGS = --strip-unneeded -R .comment -R .note -R .gnu.version -R .note.ABI-tag
# In ascending order of dubiousness.
# .note.ABI-tag stores info on which systems compatible with. May prevent executable from running on FreeBSD or OS with Linux emulation support
# .rodata stores OpenCV error messages, "basic_string::_M_construct null not valid.total() == 0 || data != NULL./usr/local/include/opencv2/core/mat.inl.hpp.Step must be a multiple of esz1...png.list.size() != 0.[NULL].stof.r.Mat.Mat."

CCs = g++-6 g++-7 g++-8 clang++


ifeq ($(CC), cc)
CC = g++
RELEASEFLAGS += -fira-loop-pressure
endif

CPPFLAGS = -Wall bpcs.cpp





debug:
	$(CC) $(CPPFLAGS) -o $(EXEPATH)_d $(STD_PARAMS) $(DEBUGFLAGS) -DEMBEDDOR   -g

release:
	$(CC) $(CPPFLAGS) -o $(EXEPATH)_   $(STD_PARAMS) $(RELEASEFLAGS)
	$(CC) $(CPPFLAGS) -o $(EXTPATH)_   $(STD_PARAMS) $(RELEASEFLAGS) -DEMBEDDOR
	strip $(STRIP_ARGS) $(EXEPATH)_
	strip $(STRIP_ARGS) $(EXTPATH)_



profile:
	$(CC) $(CPPFLAGS) -o $(EXEPATH)_prof $(STD_PARAMS) $(RELEASEFLAGS) -fprofile-generate

release-profiled:
	$(CC) $(CPPFLAGS) -o $(EXEPATH)      $(STD_PARAMS) $(RELEASEFLAGS) -fprofile-use -fprofile-correction
	strip $(STRIP_ARGS) $(EXEPATH)

release-stats:
	$(CC) $(CPPFLAGS) -o $(EXEPATH)_rs   $(STD_PARAMS) $(RELEASEFLAGS) -DRELEASE_STATS
	$(CC) $(CPPFLAGS) -o $(EXTPATH)_rs   $(STD_PARAMS) $(RELEASEFLAGS) -DRELEASE_STATS -DEMBEDDOR
	strip $(STRIP_ARGS) $(EXEPATH)_rs
	strip $(STRIP_ARGS) $(EXTPATH)_rs



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
	$(foreach cc, $(CCs), $(cc) $(EXEPATH)_$(cc)_ $(STD_PARAMS) $(RELEASEFLAGS); strip $(STRIP_ARGS) $(EXEPATH)_$(cc);)




minsrc:
	$(CC) $(CPPFLAGS) -o $(EXEPATH)_macros.cpp -E
	cat bpcs.cpp | egrep '^(#include|namespace .*\{(\n +#include .*)+\n\})' > $(EXEPATH)_macros_
	cat -s $(EXEPATH)_macros.cpp | egrep -A 99999 'typedef cv::Matx<uchar, 8, 8> Matx88uc;' | egrep -v '^# [0-9]+ "bpcs[.]cpp"' | sed 's/[(][(][(]0[)] & [(][(]1 << 3[)] - 1[)][)] + [(][(][(]1[)]-1[)] << 3[)][)]/CV_8UC1/g' >> $(EXEPATH)_macros_
	mv $(EXEPATH)_macros_ $(EXEPATH)_macros.cpp
	sed -i -r 's/\n *CV_8UC1\n *([^ ])/CV_8UC1$1/' $(EXEPATH)_macros.cpp # Regex works in Kate, not sed...
	sed -i -r 's/\{\n\n/{\n/' $(EXEPATH)_macros.cpp # same
	perl -p0e 's/\n *CV_8UC1\n *([^ ])/CV_8UC1\1/g' -i $(EXEPATH)_macros.cpp
	perl -p0e 's/\{\n *\n/{\n/g' -i $(EXEPATH)_macros.cpp
	perl -p0e 's/\n(\n *\})/\1/g' -i $(EXEPATH)_macros.cpp
	perl -p0e 's/\)\n\n/)\n/g' -i $(EXEPATH)_macros.cpp
	perl -p0e 's/(==) \n *([^ ]+)\n *\)/\1 \2)/g' -i $(EXEPATH)_macros.cpp
	perl -p0e 's/(^[v]|^v[^o]|^vo[^i]|^voi[^d]|^void[^ ].*)\{([^;}]+;)(\n *)\}\n/\1\2\3/g' -i $(EXEPATH)_macros.cpp # Remove brackets from multiline scopes
	perl -p0e 's/\n\n+/\n/g' -i $(EXEPATH)_macros.cpp # Remove excess lines
	perl -p0e 's/\(\n    (.+)\n\)/(\1)/g' -i $(EXEPATH)_macros.cpp # Move functions with single parameter to single line
	perl -p0e 's/\n *(\n *[}])/\1/g' -i $(EXEPATH)_macros.cpp # Remove lines before }
	perl -p0e 's~/\*([^/]\*|/[^\*])*\*/~~g' -i $(EXEPATH)_macros.cpp # Remove comments
	perl -p0e 's~//[^\n]*~~g' -i $(EXEPATH)_macros.cpp # Remove comments
	perl -p0e 's/case \n *([0-9]+)\n *:\n/case \1:/g' -i $(EXEPATH)_macros.cpp # Remove excess lines created when compiler expands definitions such as CV_8U
	perl -p0e 's/,\n *([^ \n])/, \1/g' -i $(EXEPATH)_macros.cpp # Remove excess lines created when macros change number of elements in a list
	perl -p0e 's/(^[v]|^v[^o]|^vo[^i]|^voi[^d]|^void[^ ].*)\{([^;}]+;)(\n *)\}\n/\1\2\3/g' -i $(EXEPATH)_macros.cpp # Remove brackets from multiline scopes (2nd pass)
	
	
	$(CC) $(CPPFLAGS) -o $(EXTPATH)_macros.cpp -E -DEMBEDDOR
	cat bpcs.cpp | egrep '^(#include|namespace .*\{(\n +#include .*)+\n\})' > $(EXTPATH)_macros_
	cat -s $(EXTPATH)_macros.cpp | egrep -A 99999 'typedef cv::Matx<uchar, 8, 8> Matx88uc;' | egrep -v '^# [0-9]+ "bpcs[.]cpp"' | sed 's/[(][(][(]0[)] & [(][(]1 << 3[)] - 1[)][)] + [(][(][(]1[)]-1[)] << 3[)][)]/CV_8UC1/g' >> $(EXTPATH)_macros_
	mv $(EXTPATH)_macros_ $(EXTPATH)_macros.cpp
	sed -i -r 's/\n *CV_8UC1\n *([^ ])/CV_8UC1$1/' $(EXTPATH)_macros.cpp # Regex works in Kate, not sed...
	sed -i -r 's/\{\n\n/{\n/' $(EXTPATH)_macros.cpp # same
	perl -p0e 's/\n *CV_8UC1\n *([^ ])/CV_8UC1\1/g' -i $(EXTPATH)_macros.cpp
	perl -p0e 's/\{\n *\n/{\n/g' -i $(EXTPATH)_macros.cpp
	perl -p0e 's/\n(\n *\})/\1/g' -i $(EXTPATH)_macros.cpp
	perl -p0e 's/\)\n\n/)\n/g' -i $(EXTPATH)_macros.cpp
	perl -p0e 's/(==) \n *([^ ]+)\n *\)/\1 \2)/g' -i $(EXTPATH)_macros.cpp
	perl -p0e 's/(^[v]|^v[^o]|^vo[^i]|^voi[^d]|^void[^ ].*)\{([^;}]+;)(\n *)\}\n/\1\2\3/g' -i $(EXTPATH)_macros.cpp # Remove brackets from multiline scopes
	perl -p0e 's/\n\n+/\n/g' -i $(EXTPATH)_macros.cpp # Remove excess lines
	perl -p0e 's/\(\n    (.+)\n\)/(\1)/g' -i $(EXTPATH)_macros.cpp # Move functions with single parameter to single line
	perl -p0e 's/\n *(\n *[}])/\1/g' -i $(EXTPATH)_macros.cpp # Remove lines before }
	perl -p0e 's~/\*([^/]\*|/[^\*])*\*/~~g' -i $(EXTPATH)_macros.cpp # Remove comments
	perl -p0e 's~//[^\n]*~~g' -i $(EXTPATH)_macros.cpp # Remove comments
	perl -p0e 's/case \n *([0-9]+)\n *:\n/case \1:/g' -i $(EXTPATH)_macros.cpp # Remove excess lines created when compiler expands definitions such as CV_8U
	perl -p0e 's/,\n *([^ \n])/, \1/g' -i $(EXTPATH)_macros.cpp # Remove excess lines created when macros change number of elements in a list
	perl -p0e 's/(^[v]|^v[^o]|^vo[^i]|^voi[^d]|^void[^ ].*)\{([^;}]+;)(\n *)\}\n/\1\2\3/g' -i $(EXTPATH)_macros.cpp # Remove brackets from multiline scopes (2nd pass)
	
	
	$(CC) $(CPPFLAGS) $(EXEPATH)_macros.cpp -o $(BUILD_DIR)/bpcs-min_$(V)   $(STD_PARAMS) $(RELEASEFLAGS)
	$(CC) $(CPPFLAGS) $(EXTPATH)_macros.cpp -o $(BUILD_DIR)/bpcs-e-min_$(V) $(STD_PARAMS) $(RELEASEFLAGS)
