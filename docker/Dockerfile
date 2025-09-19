# Builder container. Header is the same on build and run
FROM debian:stable as builder
ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=America/Recife
RUN apt-get update && \
    apt-get dist-upgrade -y && \
    apt-get install -y curl gerbv libopencv-core410 libopencv-imgproc410 libyaml-cpp0.8 libboost-filesystem1.83.0 libopencv-contrib410 zip && \
    apt-get clean

# Build-specific
RUN apt-get update && \
    apt-get install -y git libyaml-cpp-dev make g++ libopencv-dev unzip libboost-system-dev libboost-filesystem-dev && \
    apt-get clean

RUN git clone --depth 1 https://github.com/lhartmann/p2g /p2g
WORKDIR /p2g
RUN make -j$(nproc)
RUN rm -rf /p2g/html/presets
RUN strip p2g

# Runner container
FROM debian:stable
ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=America/Recife
RUN apt-get update && \
    apt-get dist-upgrade -y && \
    apt-get install -y curl gerbv libopencv-core410 libopencv-imgproc410 libyaml-cpp0.8 libboost-filesystem1.83.0 libopencv-contrib410 zip && \
    apt-get clean

# Run-specific setings
COPY --from=builder /p2g/p2g /p2g/rp2g-server.ts /usr/local/bin/
COPY --from=builder /p2g/html /usr/share/rp2g
COPY --from=builder /p2g/presets /usr/share/rp2g/presets
COPY entrypoint.sh /entrypoint.sh
#RUN chmod a+rX,go-w -R /usr/share/rp2g

EXPOSE 8644
CMD /entrypoint.sh
