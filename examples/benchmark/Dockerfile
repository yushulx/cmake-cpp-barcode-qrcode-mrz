# Linux (amd64) build/runtime environment for the three-way BarBeR benchmark.
# The Scandit Linux SDK .deb comes from the Scandit dashboard and is NOT
# redistributable; it is supplied locally at image build time only.
FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
        build-essential \
        cmake \
        git \
        ca-certificates \
        python3 \
        nlohmann-json3-dev \
        dbus \
        libdbus-1-3 \
        zlib1g \
    && rm -rf /var/lib/apt/lists/*

COPY third_party/scandit-barcodescanner-linux_8.7.1/libscandit-barcodescanner-8.7.1-amd64.deb /tmp/scandit.deb
RUN dpkg -i /tmp/scandit.deb \
    && ln -sf /usr/lib/libscanditsdk.so.8.7.1 /usr/lib/libscanditsdk.so \
    && ldconfig \
    && rm -f /tmp/scandit.deb \
    && test -f /usr/share/scanditsdk/resources/barcode_localization_default_model.wrp \
    && test -f /usr/include/Scandit/ScBarcodeScanner.h \
    && test -L /usr/lib/libscanditsdk.so

# Trial licenses write activation state here; keep it writable and persistent.
RUN mkdir -p /var/tmp/scandit /workspace
WORKDIR /workspace
