ARG BASE_IMAGE="ubuntu:22.04"
ARG TARGET_LIST="x86_64-softmmu,i386-softmmu,arm-softmmu,aarch64-softmmu,mips-softmmu,mipsel-softmmu,mips64-softmmu,mips64el-softmmu"
ARG PANDA_VERSION="pandav0.0.7"

### BASE IMAGE
FROM $BASE_IMAGE AS base
ARG BASE_IMAGE

# Copy dependencies lists into container. We copy them all and then do a mv because
# we need to transform base_image into a windows compatible filename which we can't
# do in a COPY command.
COPY ./debian/dependencies/* /tmp
RUN mv /tmp/$(echo "$BASE_IMAGE" | sed 's/:/_/g')_build.txt /tmp/build_dep.txt && \
    mv /tmp/$(echo "$BASE_IMAGE" | sed 's/:/_/g')_base.txt /tmp/base_dep.txt

# install dependencies
RUN apt-get -qq update && \
    DEBIAN_FRONTEND=noninteractive apt-get -qq install -y --no-install-recommends $(cat /tmp/base_dep.txt | grep -o '^[^#]*') $(cat /tmp/build_dep.txt | grep -o '^[^#]*') && \
    curl https://sh.rustup.rs -sSf | sh -s -- -y --profile minimal && \
    apt-get clean
ENV PATH="/root/.cargo/bin:${PATH}"

RUN pip3 install meson pycparser

ARG PANDA_VERSION
RUN wget -O /tmp/pandare.deb \
    https://github.com/panda-re/qemu/releases/download/${PANDA_VERSION}/pandare_22.04.deb && \
    apt install -yy -f /tmp/pandare.deb

RUN mkdir /panda-ng
COPY meson.build /panda-ng
COPY utils /panda-ng/utils
COPY configs /panda-ng/configs
COPY include /panda-ng/include
COPY plugins /panda-ng/plugins
COPY rust /panda-ng/rust
COPY meson.options /panda-ng

RUN mkdir -p /panda-ng/build && \
    cd /panda-ng/ && \
    meson setup build && \
    cd /panda-ng/build && \
    ninja

COPY ./python/core /panda-ng/python/core
ARG OVERRIDE_VERSION=""
RUN if [ ! -z "${OVERRIDE_VERSION}" ]; then \
        echo ${OVERRIDE_VERSION} > /panda-ng/python/core/pandare2/version.txt; \
        echo "Pretending version is ${OVERRIDE_VERSION}"; \
    fi;

RUN cd /panda-ng/python/core/ &&  \
    python3 setup.py develop

FROM base AS cleanup

RUN cd /panda-ng/python/core && python3 setup.py bdist_wheel

FROM base AS cleanup_stripped
RUN find /panda-ng/build -name "*.so" -exec strip {} \;

FROM base AS final
COPY ./python/examples /panda-ng/python/examples