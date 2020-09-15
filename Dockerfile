FROM notcompsky/static-wangle-ffmpeg-opencv:latest AS intermediate
WORKDIR /bpcs
COPY CMakeLists.txt /bpcs/CMakeLists.txt
COPY src /bpcs/src
ARG target=linux-x64
RUN mkdir -p /usr/local/include/compsky/macros \
	&& curl -sL https://raw.githubusercontent.com/NotCompsky/libcompsky/master/include/compsky/macros/likely.hpp > /usr/local/include/compsky/macros/likely.hpp \
	\
	&& mkdir /bpcs/build \
	&& mkdir /bpcs-bin \
	&& for buildtype in Release Debug RelWithDebInfo MinSizeRel; do \
		(  mkdir "/bpcs/build/$buildtype" \
		&& cd "/bpcs/build/$buildtype" \
		&& cmake \
			-DCMAKE_BUILD_TYPE="$buildtype" \
			-DNATIVE_MARCH=OFF \
			-DENABLE_STATIC=ON \
			-DBUILD_DOCS=OFF \
			-DENABLE_EXCEPTS=$(if [ $buildtype = Release ]; then echo OFF; else echo ON; fi) \
			-DCHITTY_CHATTY=$(if [ $buildtype = Debug ]; then echo ON; else echo OFF; fi) \
			-DENABLE_RUNTIME_TESTS=$(if [ $buildtype = Release ]; then echo OFF; else echo ON; fi) \
			-DMALLOC_OVERRIDE=/mimalloc-override.o \
			../.. \
		&& make \
		&& echo "Build Type: $buildtype" \
		&& ls -l bpcs* \
		&& strip -s bpcs* \
		&& ls -l bpcs* \
		&& sha1sum bpcs* \
		&& for f in bpcs bpcs-x bpcs-count bpcs-fmt; do \
			 mv "$f" "/bpcs-bin/${target}_${buildtype}_${f}"; \
		done \
	); done

FROM alpine:latest
WORKDIR /bpcs
COPY --from=intermediate /bpcs-bin/* /bpcs/
ENTRYPOINT [ "/bin/sh" ]
