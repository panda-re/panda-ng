#!/bin/bash
set -eu

# Function to get the current Ubuntu version
get_ubuntu_version() {
    lsb_release -i -s 2>/dev/null
}

if [[ $# -eq 0 ]]; then
	# No argument given, try building a package for current Ubuntu version

	# Check if we're running Ubuntu, exit otherwise
	OS=$(get_ubuntu_version)
else
	OS=$1
fi

if [[ $(get_ubuntu_version) != "Ubuntu" ]]; then
	echo "ERROR: OS of $OS is not Ubuntu and unsupported"
	exit 1
fi

if [[ $# -eq 1 ]]; then
	echo "USAGE:"
	echo "	To build a package for current Ubuntu version:"
	echo "	  $0"
	echo "	To build a package for a specific OS/version (only Ubuntu supported for now):"
	echo "	  $0 <OS> <version>"
	exit 1
fi

if [[ $# -eq 2 ]]; then
	version=$2

else
	version=$(lsb_release -r | awk '{print $2}')
fi

# Build the installer to generate the wheel file
DOCKER_BUILDKIT=1 docker build --target cleanup -t panda --build-arg BASE_IMAGE="ubuntu:${version}" ../

# Copy wheel file out of container to host
# this also preserves wheel name, which is important as pip install WILL fail if you arbitarily change the generated wheel file name
docker run --rm -v $(pwd):/out panda bash -c "cp /panda-ng/python/core/dist/pandare2-*.whl /out"

# Finish building main panda container for the target ubuntu version
# DOCKER_BUILDKIT=1 docker build --target panda -t panda --build-arg BASE_IMAGE="ubuntu:${version}" ../..

# Now build the packager container from that
docker build -t packager .

# Copy deb file out of container to host
docker run --rm -v $(pwd):/out packager bash -c "cp /pandare-plugins.deb /out"
mv pandare-plugins.deb pandare-plugins_${version}.deb
