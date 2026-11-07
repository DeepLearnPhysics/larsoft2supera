FROM ubuntu:22.04@sha256:076e7bf82319619f28dd0aa9c02ab34b055ad37100395b4f49a2ffba04fb5e11
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake git ca-certificates python3-pip \
    clang-format-14=1:14.0.0-1ubuntu1.1 clang-tidy-14=1:14.0.0-1ubuntu1.1 \
    && rm -rf /var/lib/apt/lists/*
RUN python3 -m pip install --no-cache-dir gcovr==7.2
WORKDIR /work
