FROM amd64/ubuntu:22.04 AS base

EXPOSE 9766/tcp
EXPOSE 8788/tcp

ENV DEBIAN_FRONTEND=noninteractive

# Install runtime dependencies
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
    bash wget net-tools libminiupnpc17 \
    libevent-2.1 libevent-pthreads-2.1 \
    libboost-system1.74.0 libboost-filesystem1.74.0 libboost-chrono1.74.0 \
    libboost-program-options1.74.0 libboost-thread1.74.0 \
    libzmq5 && \
    apt-get clean && rm -rf /var/lib/apt/lists/*

FROM base AS build

# Install build dependencies (gcc/g++ 11 is default in 22.04)
RUN apt-get update && \
    apt-get install -y --no-install-recommends \
    build-essential libtool autotools-dev automake \
    pkg-config libssl-dev libevent-dev bsdmainutils python3 \
    libboost-system-dev libboost-filesystem-dev libboost-chrono-dev \
    libboost-program-options-dev libboost-test-dev libboost-thread-dev \
    libzmq3-dev libminiupnpc-dev git && \
    apt-get clean && rm -rf /var/lib/apt/lists/*

# Copy source dir
COPY . /home/meowcoin/build/Meowcoin/
WORKDIR /home/meowcoin/build/Meowcoin

# Build db4 from source
WORKDIR /home/meowcoin/build/Meowcoin/contrib
RUN ./install_db4.sh ../../

# Return to main source dir
WORKDIR /home/meowcoin/build/Meowcoin

# Build Meowcoincore
RUN ./autogen.sh && \
    ./configure --disable-tests --enable-sse2 \
        BDB_LIBS="-L/home/meowcoin/build/db4/lib -ldb_cxx-4.8" \
        BDB_CFLAGS="-I/home/meowcoin/build/db4/include" \
        --with-gui=no \
        CXXFLAGS="-std=c++17" \
        LDFLAGS="-std=c++17" && \
    make -j$(nproc) V=1

FROM base AS final

# Add our service account user
RUN useradd -ms /bin/bash meowcoin && \
    mkdir /var/lib/meowcoin && \
    chown meowcoin:meowcoin /var/lib/meowcoin && \
    ln -s /var/lib/meowcoin /home/meowcoin/.meowcoin && \
    chown -h meowcoin:meowcoin /home/meowcoin/.meowcoin

VOLUME /var/lib/meowcoin

# Copy the compiled binaries from the build stage
COPY --from=build /home/meowcoin/build/Meowcoin/src/meowcoind /usr/local/bin/meowcoind
COPY --from=build /home/meowcoin/build/Meowcoin/src/meowcoin-cli /usr/local/bin/meowcoin-cli

WORKDIR /home/meowcoin
USER meowcoin

CMD ["/usr/local/bin/meowcoind", "-datadir=/var/lib/meowcoin", "-printtoconsole", "-onlynet=ipv4"]