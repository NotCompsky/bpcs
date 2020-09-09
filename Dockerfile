FROM notcompsky/static-wangle-ffmpeg-opencv:latest AS intermediate
WORKDIR /bpcs
COPY CMakeLists.txt /bpcs/CMakeLists.txt
COPY src /bpcs/src
RUN ln -s /usr/local/include/opencv4/opencv2 /usr/local/include/opencv2 \
	&& mkdir -p /usr/local/include/compsky/macros \
	&& curl -sL https://raw.githubusercontent.com/NotCompsky/libcompsky/master/include/compsky/macros/likely.hpp > /usr/local/include/compsky/macros/likely.hpp \
	&& mkdir /bpcs/build \
	&& cd /bpcs/build \
	&& cmake \
		-DCMAKE_BUILD_TYPE=Debug \
		-DNATIVE_MARCH=OFF \
		-DENABLE_STATIC=ON \
		-DOPENCV_3RDPARTY_LIBDIR=/usr/local/lib64/opencv4/3rdparty \
		-DBUILD_DOCS=OFF \
		-DENABLE_EXCEPTS=ON \
		-DCHITTY_CHATTY=OFF \
		-DENABLE_RUNTIME_TESTS=OFF \
		-DMALLOC_OVERRIDE=/mimalloc-override.o \
		\
		-DGRID_W=9 \
		-DGRID_H=9 \
		-DN_CHANNELS=3 \
		-DMAX_BIT_DEPTH=16 \
		.. \
	&& make \
	&& ls -l bpcs* \
	&& strip -s bpcs* \
	&& ls -l bpcs*

FROM alpine:latest
WORKDIR /bpcs
COPY --from=intermediate /bpcs/build/bpcs* /bpcs/
ENTRYPOINT [ "/bin/sh" ]
