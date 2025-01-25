# Copyright 2022 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

FROM ubuntu:22.04

# install system deps
RUN apt-get update && apt-get install -y build-essential wget vim libstdc++-10-dev \
clang git checkinstall zlib1g-dev curl protobuf-compiler cmake \
pkg-config libdbus-1-dev ninja-build libssl-dev default-jre
RUN apt upgrade -y

RUN wget https://dl.google.com/linux/direct/google-chrome-stable_current_amd64.deb
RUN apt-get -y install ./google-chrome-stable_current_amd64.deb
RUN export CHROME_BIN="/usr/bin/google-chrome-stable"

ENV CC=/usr/bin/clang
ENV CXX=/usr/bin/clang++

# install cargo with default settings
RUN curl https://sh.rustup.rs -sSf | sh -s -- -y --default-toolchain 1.77.1
ENV PATH="/root/.cargo/bin:${PATH}"
RUN rustup install stable
RUN rustup toolchain install nightly --force
RUN rustup component add rust-src --toolchain nightly-x86_64-unknown-linux-gnu
RUN rustup target add wasm32-unknown-unknown thumbv7m-none-eabi

RUN cargo install --locked cargo-deny --color never 2>&1
RUN cargo install --locked cargo-llvm-cov --color never 2>&1
RUN cargo install bindgen-cli --version 0.69.4 --color never 2>&1
RUN cargo install wasm-pack --color never 2>&1

# unreleased PR https://github.com/ehuss/cargo-prefetch/pull/6
RUN cargo install cargo-prefetch \
    --git https://github.com/marshallpierce/cargo-prefetch.git \
    --rev f6affa68e950275f9fd773f2646ab7ee4db82897 \
    --color never 2>&1

# boringssl build wants go
RUN curl -L https://go.dev/dl/go1.20.2.linux-amd64.tar.gz | tar -C /usr/local -xz
ENV PATH="$PATH:/usr/local/go/bin"

# needed for boringssl git operations
RUN git config --global user.email "docker@example.com"
RUN git config --global user.name "NP Docker"

# Download google-java-format
RUN mkdir /opt/google-java-format
WORKDIR /opt/google-java-format
ENV GOOGLE_JAVA_FORMAT_VERSION="1.19.2"
RUN wget "https://github.com/google/google-java-format/releases/download/v${GOOGLE_JAVA_FORMAT_VERSION}/google-java-format-${GOOGLE_JAVA_FORMAT_VERSION}-all-deps.jar" -q -O "google-java-format-all-deps.jar"

RUN mkdir -p /beto-core
COPY . /beto-core
WORKDIR /beto-core

# prefetch dependencies so later build steps don't re-download on source changes
RUN cargo prefetch --lockfile nearby/Cargo.lock 2>&1
RUN cargo prefetch --lockfile common/Cargo.lock 2>&1

# when the image runs build and test everything to ensure env is setup correctly
WORKDIR /beto-core/nearby
CMD ["cargo", "run", "--", "check-everything"]
