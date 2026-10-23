# Sourced helper: extract an upstream QEMU release tarball.
#
# The Arc CI runners ship `tar` but NOT the `xz` binary, so `tar xf foo.tar.xz`
# dies with "tar (child): xz: Cannot exec". Every workflow here already depends
# on python3, whose stdlib lzma module needs no external binary -- so decompress
# through it and pipe a plain tar stream. Prefer real xz when present (faster,
# and it is what a developer's machine will use).
extract_tarball() {
    local tarball="$1" dest="$2"
    mkdir -p "$dest"
    case "$tarball" in
        *.tar.xz|*.txz)
            if command -v xz >/dev/null 2>&1; then
                tar xf "$tarball" -C "$dest" --strip-components=1
            else
                python3 -c 'import lzma,sys,shutil; shutil.copyfileobj(lzma.open(sys.argv[1],"rb"), sys.stdout.buffer)' \
                    "$tarball" | tar x -C "$dest" --strip-components=1
            fi
            ;;
        *)
            tar xf "$tarball" -C "$dest" --strip-components=1
            ;;
    esac
}
