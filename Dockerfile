FROM notcompsky/static-wangle-ffmpeg-opencv:latest AS intermediate
WORKDIR /bpcs
COPY CMakeLists.txt /bpcs/CMakeLists.txt
COPY src /bpcs/src
RUN ln -s /usr/local/include/opencv4/opencv2 /usr/local/include/opencv2 \
	&& mkdir /bpcs/build \
	&& cd /bpcs/build \
	&& cmake \
		-DCMAKE_BUILD_TYPE=Release \
		-DNATIVE_MARCH=OFF \
		-DENABLE_STATIC=ON \
		-DOPENCV_3RDPARTY_LIBDIR=/usr/local/lib64/opencv4/3rdparty \
		-DBUILD_DOCS=OFF \
		.. \
	&& make \
	&& ls -l bpcs*

FROM alpine:latest
WORKDIR /bpcs
COPY --from=intermediate /bpcs/build/bpcs* /bpcs/
ENTRYPOINT [ "/bin/sh" ]
