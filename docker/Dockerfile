# Builder container. Header is the same on build and run
FROM debian:stable as builder
ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=America/Recife
RUN apt-get update && \
    apt-get dist-upgrade -y && \
    apt-get install -y curl gerbv libopencv-core406 libopencv-imgproc406 libyaml-cpp0.7 libboost-filesystem1.74.0 libopencv-contrib406 zip && \
    apt-get clean

# Build-specific
RUN apt-get update && \
    apt-get install -y libyaml-cpp-dev make g++ libopencv-dev unzip libboost-system-dev libboost-filesystem-dev && \
    apt-get clean

ADD p2g.tar.xz /p2g
WORKDIR /p2g
RUN make -j$(nproc)
RUN strip p2g

# Runner container
FROM debian:stable
ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=America/Recife
RUN apt-get update && \
    apt-get dist-upgrade -y && \
    apt-get install -y curl gerbv libopencv-core406 libopencv-imgproc406 libyaml-cpp0.7 libboost-filesystem1.74.0 libopencv-contrib406 zip && \
    apt-get clean

# Run-specific setings
COPY --from=builder /p2g/p2g /p2g/rp2g-server.ts /usr/local/bin/
COPY --from=builder /p2g/html /usr/share/rp2g
COPY --from=builder /p2g/presets /usr/share/rp2g/presets
COPY entrypoint.sh /entrypoint.sh
#RUN chmod a+rX,go-w -R /usr/share/rp2g

EXPOSE 8644
CMD /entrypoint.sh
