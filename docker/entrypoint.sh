#! /bin/sh

# Install packages, if not done via Dockerfile
gerbv --version || {
    apt-get update
    apt-get dist-upgrade -y
    apt-get install -y curl gerbv libopencv-core406 libopencv-imgproc406 libyaml-cpp0.7 libboost-filesystem1.74.0 libopencv-contrib406 zip
    apt-get clean
}

# Install DENO, if not done via Dockerfile
deno --version || {
    URL=https://github.com/denoland/deno/releases/download/v1.22.3/deno-x86_64-unknown-linux-gnu.zip
    DENO=/usr/local/bin/deno

    curl -L $URL | zcat > $DENO
    chmod a+x $DENO
    deno upgrade
}

/usr/local/bin/rp2g-server.ts --webroot=/usr/share/rp2g
