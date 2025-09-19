#syntax=docker/dockerfile:1.17-labs
ARG REGISTRY="docker.io"
ARG BASE_IMAGE="ubuntu:22.04"
ARG TARGET_LIST="x86_64-softmmu,i386-softmmu,arm-softmmu,aarch64-softmmu,mips-softmmu,mipsel-softmmu,mips64-softmmu,mips64el-softmmu"
ARG PANDA_VERSION="pandav0.0.46"

### BASE IMAGE
FROM ${REGISTRY}/$BASE_IMAGE AS base
ARG BASE_IMAGE

# Copy dependencies lists into container. We copy them all and then do a mv because
# we need to transform base_image into a windows compatible filename which we can't
# do in a COPY command.
COPY ./debian/dependencies/${BASE_IMAGE/:/_}_base.txt /tmp/base_dep.txt
COPY ./debian/dependencies/${BASE_IMAGE/:/_}_build.txt /tmp/build_dep.txt

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
    pip install -r requirements.txt && \
    python3 setup.py install

FROM base AS cleanup

RUN cd /panda-ng/python/core && python3 setup.py bdist_wheel

FROM base AS cleanup_stripped
RUN find /panda-ng/build -name "*.so" -exec strip {} \;
RUN find /panda-ng/build/plugins -name "*.o" -print0 | xargs -0 rm -f
# delete large rust artifacts we do not need
RUN rm -rf /panda-ng/build/plugins/rust

FROM base AS packager

# Install necessary tools for packaging
RUN apt-get -qq update && \
    DEBIAN_FRONTEND=noninteractive apt-get -qq install -y \
        fakeroot dpkg-dev

RUN mkdir /plugins
# Get dependencies list from base image
COPY --from=base /tmp/base_dep.txt /tmp
COPY --from=base /tmp/build_dep.txt /tmp
COPY --from=cleanup_stripped /panda-ng/build/plugins /plugins
# Set up /package-root with files from panda we'll package
RUN cd /plugins && rm -rf *.a *.o *.so.p && \
    mkdir -p /package-root/usr/local/lib/panda/panda/plugins && \
    cp -r /plugins/* /package-root/usr/local/lib/panda/panda/plugins

# Create DEBIAN directory and control file
COPY ./debian/control /package-root/DEBIAN/control

# Update control file with dependencies
# Build time. We only select dependencies that are not commented out or blank
RUN dependencies=$(grep '^[a-zA-Z]' /tmp/build_dep.txt | tr '\n' ',' | sed 's/,,\+/,/g'| sed 's/,$//') && \
    sed -i "s/BUILD_DEPENDS_LIST/Build-Depends: $dependencies/" /package-root/DEBIAN/control

# Run time. Also includes ipxe-qemu so we can get pc-bios files
RUN dependencies=$(grep '^[a-zA-Z]' /tmp/base_dep.txt | tr '\n' ',' | sed 's/,,\+/,/g' | sed 's/,$//') && \
    sed -i "s/DEPENDS_LIST/Depends: ipxe-qemu,${dependencies}/" /package-root/DEBIAN/control

# Build the package
RUN fakeroot dpkg-deb --build /package-root /pandare-plugins.deb

# The user can now extract the .deb file from the container with something like
# docker run --rm -v $(pwd):/out packager bash -c "cp /pandare.deb /out"

FROM cleanup_stripped AS final
COPY ./python/examples /panda-ng/python/examples