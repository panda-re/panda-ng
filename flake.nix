{
  description = "panda-next: the PANDA engine (patched QEMU built with --enable-panda), its generated headers, and plugins";

  inputs = {
    # Pinned by revision. This is the revision qemu_builder (and penguin) pinned when the engine
    # moved here; panda-ng no longer follows penguin's pin and bumps it on its own schedule (a bump
    # rebuilds the engine and everything on it: rerun the guest tests).
    nixpkgs.url = "github:NixOS/nixpkgs/b6067cc0127d4db9c26c79e4de0513e58d0c40c9";
  };

  outputs =
    {
      self,
      nixpkgs,
      ...
    }:
    let
      systems = [
        "x86_64-linux"
        "aarch64-linux"
      ];
      forAllSystems = nixpkgs.lib.genAttrs systems;
      # CI writes the release version to VERSION before building.
      version = nixpkgs.lib.fileContents ./VERSION;

      defaultTargets = [
        "x86_64-softmmu"
        "i386-softmmu"
        "arm-softmmu"
        "aarch64-softmmu"
        "mips-softmmu"
        "mipsel-softmmu"
        "mips64-softmmu"
        "mips64el-softmmu"
        "ppc-softmmu"
        "ppc64-softmmu"
        "loongarch64-softmmu"
        "riscv32-softmmu"
        "riscv64-softmmu"
      ];
      # x86_64, i386, ...: the <arch> in qemu-system-<arch> and libpanda-<arch>-softmmu.so.
      arches = map (nixpkgs.lib.removeSuffix "-softmmu") defaultTargets;

      # Everything this flake builds, from any nixpkgs (the overlay passes the consumer's).
      mkPandaNext =
        pkgs:
        let
          lib = pkgs.lib;
          system = pkgs.stdenv.hostPlatform.system;

          # libpanda-ng, the header generator (tools/libpanda-ng, from panda-re/libpanda-ng 7b257bb
          # plus local fixes). Only what the build runs, so other tree changes (or README edits)
          # don't rebuild the engine.
          libpanda-ng = lib.fileset.toSource {
            root = ./tools/libpanda-ng;
            fileset = lib.fileset.difference ./tools/libpanda-ng ./tools/libpanda-ng/README.md;
          };

          gdbPythonPath = pkgs.python3.pkgs.makePythonPath (
            with pkgs.python3.pkgs;
            [
              cffi
              pycparser
              tree-sitter
              tree-sitter-grammars.tree-sitter-c
            ]
          );

          # Linked into the wheel's engine: nix's glibc (2.42) gave cfset[io]speed new versions
          # that older hosts don't have. See nix/glibc-compat/termios-compat.c.
          glibcCompat = pkgs.runCommandCC "panda-glibc-compat" { } ''
            mkdir -p $out
            $CC -O2 -fPIC -c ${./nix/glibc-compat/termios-compat.c} -o $out/termios-compat.o
          '';

          # The engine's source: engine/'s upstream tarball + patch series + src/ overlay
          # (engine/nix/source.nix, engine/README.md).
          engineSource = import ./engine/nix/source.nix { inherit pkgs; };

          # The engine. Keeps nothing of the build tree but what it installs, plus the headers
          # libpanda-ng generates from it (which need the tree, so they are made here).
          mkEngine =
            {
              targets ? defaultTargets,
              # Portable to older glibc (the wheel's engine): see glibcCompat.
              portable ? false,
            }:
            pkgs.stdenv.mkDerivation {
              pname = "panda-engine" + lib.optionalString portable "-portable";
              inherit version;
              src = engineSource.src;
              # sdk: what an external board or device (a panda module) compiles against; mkBoard.
              outputs = [
                "out"
                "sdk"
              ];

              nativeBuildInputs = with pkgs; [
                # QEMU's bundled meson drops nix's rpaths at install; put them back.
                autoPatchelfHook
                pkg-config
                ninja
                perl
                git
                gdb
                (python3.withPackages (ps: [
                  ps.setuptools
                  ps.wheel
                  ps.pip
                  ps.cffi
                  ps.tree-sitter
                  ps.tree-sitter-grammars.tree-sitter-c
                ]))
              ];
              buildInputs = with pkgs; [
                glib
                pixman
                zlib
                zstd
                libcap_ng
                libslirp
                dtc
              ];

              # The source is read-only in the store; configure writes pyvenv/ and build/.
              unpackPhase = ''
                cp -r $src source
                chmod -R u+w source
                cd source
                patchShebangs scripts
              '';

              configurePhase = ''
                runHook preConfigure
                mkdir build && cd build
                ../configure --prefix=$out \
                  --target-list=${lib.concatStringsSep "," targets} \
                  --enable-plugins --enable-panda \
                  --disable-docs --disable-werror --disable-gtk --disable-sdl --disable-opengl \
                  --disable-virglrenderer --disable-spice --disable-vnc --disable-containers${lib.optionalString portable " --extra-ldflags=${glibcCompat}/termios-compat.o"}
                cd ..
                runHook postConfigure
              '';

              buildPhase = ''
                runHook preBuild
                ninja -C build -j$NIX_BUILD_CORES
                # libpanda-ng: gdb reads each build/libpanda-<target>.so and writes
                # panda_{c,cpp,...}_<target>.h next to its scripts.
                # gdb embeds its own python3 (same nixpkgs); give it build.py's modules.
                cp -r --no-preserve=mode ${libpanda-ng} ../libpanda-ng
                (cd ../libpanda-ng && PYTHONPATH=${gdbPythonPath} bash run_all.sh $PWD/../source)
                runHook postBuild
              '';

              installPhase = ''
                runHook preInstall
                ninja -C build install
                cp build/contrib/plugins/libpanda_plugin_interface.so $out/lib/
                # The board SDK: headers, per-target cflags, panda-board-cc (scripts/panda-sdk.sh).
                bash scripts/panda-sdk.sh build $sdk
                mkdir -p $out/include/panda-ng
                cp ../libpanda-ng/*.h $out/include/panda-ng/
                # libqemu-system-<name>.so (with penguin's alias names), libqemu-kvm-<target>.so and
                # libpanda-<target>-softmmu.so are links of the same objects; only the soname differs,
                # and all of them are dlopened by path. Keep libpanda-* and link the rest to it.
                declare -A alias=([intel64]=x86_64 [armel]=arm [mipseb]=mips [mips64eb]=mips64
                  [powerpc]=ppc [powerpc64]=ppc64 [powerpc64le]=ppc64)
                for f in $out/lib/libqemu-system-*.so $out/lib/libqemu-kvm-*.so; do
                  name=''${f##*/libqemu-system-}; name=''${name##*/libqemu-kvm-}; name=''${name%.so}
                  target=''${alias[$name]:-$name}
                  if [ -f $out/lib/libpanda-$target-softmmu.so ]; then
                    ln -sf libpanda-$target-softmmu.so $f
                  fi
                done
                runHook postInstall
              '';

              # After autoPatchelf (fixup): the engine has to load its libraries.
              doInstallCheck = true;
              installCheckPhase = ''
                $out/bin/qemu-system-${lib.removeSuffix "-softmmu" (builtins.head targets)} --version
              '';
            };

          engine = mkEngine { };

          # mkBoard: an external board (or device) shared object built with an engine's SDK, for
          # -M <out>/lib/panda-boards/<name>.so[:MACHINE] (or -module). The load check runs the
          # engine on it, so a board built against the wrong SDK fails here, not in a run.
          mkBoard =
            {
              name,
              target, # e.g. "arm-softmmu"
              src, # a directory of *.c (and headers)
              engine ? mkEngine { },
              extraCflags ? [ ],
            }:
            pkgs.stdenv.mkDerivation {
              pname = "panda-board-${name}";
              inherit version src;
              nativeBuildInputs = [ pkgs.python3 ];
              dontConfigure = true;
              buildPhase = ''
                runHook preBuild
                mkdir -p $out/lib/panda-boards
                ${engine.sdk}/bin/panda-board-cc ${target} -o $out/lib/panda-boards/${name}.so \
                  ${lib.concatStringsSep " " extraCflags} $(find . -name '*.c')
                runHook postBuild
              '';
              dontInstall = true;
              doInstallCheck = true;
              installCheckPhase = ''
                ${engine}/bin/qemu-system-${lib.removeSuffix "-softmmu" target} \
                  -M $out/lib/panda-boards/${name}.so,help > /dev/null 2> err || \
                  ${engine}/bin/qemu-system-${lib.removeSuffix "-softmmu" target} \
                  -module $out/lib/panda-boards/${name}.so -M none,help > /dev/null
              '';
            };

          pluginTools = {
            nativeBuildInputs = with pkgs; [
              pkg-config
              meson
              ninja
              cargo
              rustc
            ];
            buildInputs = with pkgs; [
              glib
              curl
              # osi_isf reads xz-compressed ISF JSON.
              nlohmann_json
              xz
              (python3.withPackages (ps: [ ps.pycparser ]))
            ];
          };

          # panda-ng's plugins, built against an engine's generated headers.
          mkPlugins =
            engine:
            pkgs.stdenv.mkDerivation {
              pname = "panda-plugins";
              inherit version;
              src = lib.fileset.toSource {
                root = ./.;
                fileset = lib.fileset.unions [
                  ./meson.build
                  ./meson.options
                  ./configs
                  ./include
                  ./plugins
                  ./rust
                  ./utils
                ];
              };
              inherit (pluginTools) nativeBuildInputs buildInputs;
              NIX_CFLAGS_COMPILE = "-std=c++17";
              mesonFlags = [ (lib.mesonOption "targets" (lib.concatStringsSep "," defaultTargets)) ];
              preConfigure = ''
                mkdir -p local_packages
                ln -s ${engine}/include/panda-ng local_packages/panda-ng
              '';
              # meson installs only the rust plugins. Install the build tree's layout, which is what
              # libpanda's plugin search and syscalls2 (dso_info libraries in syscalls2/) expect.
              installPhase = ''
                runHook preInstall
                mkdir -p $out/lib/panda/plugins
                (cd plugins && find . -name '*.so' -not -path './rust/*' -not -path '*.p/*' \
                  -exec cp --parents -t $out/lib/panda/plugins {} +)
                runHook postInstall
              '';
            };
          plugins = mkPlugins engine;

          python = pkgs.python3;
          pluginDir = "${plugins}/lib/panda/plugins";

          # pandare2's sources: setup.py reads python/core and the plugins' PPP headers.
          pandare2Src = lib.fileset.toSource {
            root = ./.;
            fileset = lib.fileset.unions [
              ./python/core
              ./plugins
              # python/core/LICENSE and LICENSES are symlinks to these (setup.cfg's license_files).
              ./LICENSE
              ./LICENSES
            ];
          };

          # pandare2 for a nixpkgs Python package set, with this engine and these plugins ahead of
          # its /usr/local defaults.
          mkPandare2 =
            ps:
            ps.buildPythonPackage {
              pname = "pandare2";
              inherit version;
              src = pandare2Src;
              format = "setuptools";
              sourceRoot = "source/python/core";
              # setup.py generates the cffi bindings from the engine's headers and plugins/.
              postUnpack = ''
                chmod -R u+w source
                mkdir -p source/local_packages
                ln -s ${engine}/include/panda-ng source/local_packages/panda-ng
              '';
              postPatch = ''
                echo ${version} > pandare2/version.txt
                # pandare2.mkimage generates osi_linux kernelinfo with it.
                cp ../../plugins/osi_linux/utils/kernelinfo_gdb/extract_kernelinfo.py pandare2/
                substituteInPlace pandare2/mkimage.py \
                  --replace-fail '[shutil.which("gdb-multiarch")' '["${pkgs.gdb}/bin/gdb", shutil.which("gdb-multiarch")' \
                  --replace-fail 'shutil.which("dwarf2json")' '"${pkgs.dwarf2json}/bin/dwarf2json"'
                substituteInPlace pandare2/utils.py \
                  --replace-fail 'pot_paths = [pjoin(python_package, "lib"),' 'pot_paths = ["${engine}/lib", pjoin(python_package, "lib"),'
                # The plugin interface and firmware are found next to libpanda (lib/, share/qemu/).
                substituteInPlace pandare2/panda.py \
                  --replace-fail 'realpath(pjoin(__file__, "../../../../build/plugins")),' \
                    '"${pluginDir}", realpath(pjoin(__file__, "../../../../build/plugins")),' \
                  --replace-fail 'f"/usr/local/lib/panda/{self.arch_name}"' '"${pluginDir}"'
              '';
              build-system = [
                ps.setuptools
                ps.cffi
              ];
              dependencies = [ ps.cffi ];
              pythonImportsCheck = [ "pandare2" ];
            };
          pandare2 = mkPandare2 python.pkgs;

          # The Python users run PANDA from, with pandare2.
          pandaPython = python.withPackages (ps: [ pandare2 ]);

          # qemu-system-<arch> for every target, as a thin launcher over libpanda-<arch>-softmmu.so
          # (launcher/panda-qemu.c), plus the engine's qemu-img/-io/-nbd. Layout bin/, lib/,
          # share/qemu/ under one prefix: the launcher finds the engine in ../lib, QEMU its
          # firmware in ../share/qemu.
          mkQemu =
            engine:
            pkgs.stdenv.mkDerivation {
              pname = "panda-qemu";
              inherit version;
              src = ./launcher;
              buildPhase = ''
                runHook preBuild
                $CC -O2 -Wall -Werror -o panda-qemu panda-qemu.c
                runHook postBuild
              '';
              installPhase = ''
                runHook preInstall
                mkdir -p $out/bin $out/share
                cp panda-qemu $out/bin/
                for a in ${lib.concatStringsSep " " arches}; do
                  ln -s panda-qemu $out/bin/qemu-system-$a
                done
                for t in qemu-img qemu-io qemu-nbd; do
                  ln -s ${engine}/bin/$t $out/bin/$t
                done
                ln -s ${engine}/lib $out/lib
                ln -s ${engine}/share/qemu $out/share/qemu
                runHook postInstall
              '';
              meta = {
                description = "The PANDA engine as plain QEMU (qemu-system-<arch> for each target)";
                mainProgram = "qemu-system-x86_64";
              };
            };
          qemu = mkQemu engine;
          # One package per arch: `nix run .#qemu-system-riscv64 -- ...`.
          qemuSystem = lib.genAttrs (map (a: "qemu-system-${a}") arches) (
            name:
            pkgs.runCommand "panda-${name}-${version}"
              {
                meta = {
                  description = "The PANDA engine as ${name}";
                  mainProgram = name;
                };
              }
              ''
                mkdir -p $out/bin
                ln -s ${qemu}/bin/${name} $out/bin/${name}
              ''
          );

          # Everything a user installs under one prefix: the engine (with its plugin-interface
          # library and headers), the plugins, and a Python with pandare2. bin/ adds
          # panda-python and panda-system-<arch>, the names the .deb puts in /usr/bin.
          dist = pkgs.symlinkJoin {
            name = "panda-next-${version}";
            paths = [
              engine
              plugins
              pandaPython
            ];
            postBuild = ''
              # nix's Python doesn't look in the host's CA paths: use the host's bundle when there
              # is one (it has local CAs), else nix's.
              cat > $out/bin/panda-python <<EOF
              #!${pkgs.runtimeShell}
              if [ -z "\''${SSL_CERT_FILE:-}" ]; then
                if [ -r /etc/ssl/certs/ca-certificates.crt ]; then
                  export SSL_CERT_FILE=/etc/ssl/certs/ca-certificates.crt
                else
                  export SSL_CERT_FILE=${pkgs.cacert}/etc/ssl/certs/ca-bundle.crt
                fi
              fi
              exec $out/bin/python3 "\$@"
              EOF
              chmod +x $out/bin/panda-python
              for b in $out/bin/qemu-system-*; do
                ln -s $(basename $b) $out/bin/panda-system-''${b##*/qemu-system-}
              done
            '';
          };

          # A .deb of dist's runtime closure, copied as is into /nix/store, with dist linked to
          # /opt/panda-next. It needs nothing from the host's packages (not even its glibc), so one
          # .deb per arch installs on any Debian or Ubuntu release. Only names no distro package
          # uses go in /usr/bin.
          debArch =
            {
              x86_64-linux = "amd64";
              aarch64-linux = "arm64";
            }
            .${system} or (throw "panda-next: no .deb architecture for ${system}");
          deb =
            pkgs.runCommand "panda-next_${version}_${debArch}.deb"
              {
                nativeBuildInputs = [ pkgs.dpkg ];
                closure = pkgs.closureInfo { rootPaths = [ dist ]; };
              }
              ''
                root=$TMPDIR/root
                mkdir -p $root/nix/store $root/opt $root/usr/bin $root/DEBIAN
                xargs -a $closure/store-paths cp -a -t $root/nix/store/
                ln -s ${dist} $root/opt/panda-next
                for b in ${dist}/bin/panda-*; do
                  ln -s /opt/panda-next/bin/$(basename $b) $root/usr/bin/
                done
                cat > $root/DEBIAN/control <<EOF
                Package: panda-next
                Version: ${version}
                Architecture: ${debArch}
                Maintainer: PANDA developers
                Description: PANDA engine (QEMU with the PANDA API), plugins and pandare2
                 Built with nix. The runtime closure is under /nix/store; the install is at
                 /opt/panda-next. Run pandare2 scripts with panda-python.
                EOF
                chmod -R u+w $root
                dpkg-deb --root-owner-group -Zxz --build $root $out
              '';

          # The pandare2 wheel for pip outside nix: py3-none-manylinux_2_<N>_<arch>, with the
          # engine (built against glibc-compat), plugins, firmware and the qemu-system launcher
          # inside, and every non-glibc library they need bundled (tools/wheel/repair.py).
          enginePortable = mkEngine { portable = true; };
          pluginsPortable = mkPlugins enginePortable;
          qemuPortable = mkQemu enginePortable;

          # What the wheel ships besides pandare2, with the license of the part it ships (not
          # nixpkgs' meta.license, which covers the whole package: util-linux's libmount is
          # LGPL, its tools GPL). repair.py fails if it bundles a library from a store path none
          # of these packages' outputs own, and copies each used entry's license files (from the
          # upstream source: COPYING*, LICENSE*, ...) into the wheel's dist-info/licenses/<name>/.
          wheelThirdParty = with pkgs; [
            { pkg = glib; spdx = "LGPL-2.1-or-later"; }
            { pkg = pixman; spdx = "MIT"; }
            { pkg = zlib; spdx = "Zlib"; }
            { pkg = zstd; spdx = "BSD-3-Clause OR GPL-2.0-only"; }
            { pkg = libslirp; spdx = "BSD-3-Clause"; }
            {
              pkg = dtc;
              spdx = "GPL-2.0-or-later OR BSD-2-Clause"; # libfdt
              files = [ "README.license" "BSD-2-Clause" "GPL" ];
            }
            { pkg = libcap_ng; spdx = "LGPL-2.1-or-later"; }
            { pkg = pcre2; spdx = "BSD-3-Clause WITH PCRE2-exception"; }
            { pkg = libffi; spdx = "MIT"; }
            { pkg = util-linuxMinimal; spdx = "LGPL-2.1-or-later"; } # libblkid, libmount
            { pkg = libselinux; spdx = "LicenseRef-Public-Domain"; }
            { pkg = stdenv.cc.cc; spdx = "GPL-3.0-or-later WITH GCC-exception-3.1"; } # libstdc++, libgcc_s
            { pkg = xz; spdx = "0BSD"; } # liblzma
            { pkg = curl; spdx = "curl"; }
            { pkg = nghttp2; spdx = "MIT"; }
            { pkg = nghttp3; spdx = "MIT"; }
            { pkg = ngtcp2; spdx = "MIT"; }
            { pkg = libidn2; spdx = "LGPL-3.0-or-later OR GPL-2.0-or-later"; src = libidn2.passthru.out.src; }
            { pkg = libunistring; spdx = "LGPL-3.0-or-later OR GPL-2.0-or-later"; }
            { pkg = libpsl; spdx = "MIT"; }
            { pkg = libssh2; spdx = "BSD-3-Clause"; }
            { pkg = openssl; spdx = "Apache-2.0"; }
            { pkg = krb5; spdx = "MIT"; }
            { pkg = keyutils; spdx = "LGPL-2.1-or-later"; } # libkeyutils
            { pkg = brotli; spdx = "MIT"; }
          ];
          # repair.py's --licenses dir: manifest.json plus <name>/<license files>. The engine and
          # panda-ng are always in it; SOURCES (written by repair.py) says where the corresponding
          # source of each is.
          wheelLicenses =
            let
              noCtx = builtins.unsafeDiscardStringContext;
              # mirror://gnome/... -> the first https mirror.
              mirrors = import (pkgs.path + "/pkgs/build-support/fetchurl/mirrors.nix");
              unmirror =
                u:
                let
                  m = builtins.match "mirror://([^/]+)/(.*)" u;
                in
                if m == null then u else builtins.head mirrors.${builtins.head m} + builtins.elemAt m 1;
              third = map (e: rec {
                name = e.pkg.pname or (builtins.parseDrvName e.pkg.name).name;
                version = e.pkg.version or "";
                inherit (e) spdx;
                src = e.src or e.pkg.src;
                files = e.files or [ ];
                urls = lib.unique (map unmirror (src.urls or [ (src.url or "") ]));
                prefixes = map (o: noCtx (toString e.pkg.${o})) e.pkg.outputs;
              }) wheelThirdParty;
              engineSrc = engineSource.src;
              manifest = {
                engine = {
                  name = "qemu-panda-engine";
                  spdx = "GPL-2.0-only";
                  source = [
                    "QEMU ${engineSource.version} with the PANDA patch series, from panda-ng's engine/"
                    "  repository: https://github.com/panda-re/panda-ng"
                    "  commit: ${self.rev or self.dirtyRev or "unknown (not built from a git checkout)"}"
                    "  upstream: ${engineSource.base.url or "https://download.qemu.org/qemu-${engineSource.base.tag}.tar.xz"} (sha256 ${engineSource.base.sha256})"
                    "  patch series: engine/patches/${engineSource.version}/series in that commit (the PANDA overlay is engine/src/)"
                    "  patched tree as built: ${noCtx (toString engineSrc)} (nix: engine/nix/source.nix)"
                    "  firmware in pandare2/data/share/qemu comes from that tree's pc-bios/ (see its LICENSE and pc-bios/README;"
                    "  firmware sources are in the roms/ submodules of upstream QEMU ${engineSource.version})"
                  ];
                };
                panda-ng = {
                  name = "panda-ng";
                  # REUSE.toml: panda-ng's own code is MIT; pandare2's modules from classic PANDA are
                  # GPL-2.0-or-later, the plugins from classic PANDA and the generated bindings
                  # GPL-2.0-only. Its license files are LICENSE, LICENSES/ and REUSE.toml.
                  spdx = "MIT AND GPL-2.0-only AND GPL-2.0-or-later";
                  source = [
                    "panda-ng (pandare2, the PANDA plugins, the qemu-system launcher)"
                    "  repository: https://github.com/panda-re/panda-ng"
                    "  commit: ${self.rev or self.dirtyRev or "unknown (not built from a git checkout)"}"
                    "  (a -dirty commit: release builds write VERSION into the tree before building)"
                  ];
                };
                thirdParty = map (t: {
                  inherit (t) name version spdx urls prefixes;
                }) third;
              };
            in
            pkgs.runCommand "pandare2-wheel-licenses-${version}"
              {
                nativeBuildInputs = [ pkgs.lzip ];
                manifest = builtins.toJSON manifest;
                passAsFile = [ "manifest" ];
              }
              ''
                mkdir -p $out
                cp $manifestPath $out/manifest.json
                # Top-level license files of a source tarball or directory.
                # Extra file names (beyond COPYING*, LICENSE*, ...) after the source.
                licenses() {
                  local name=$1 src=$2 x=$TMPDIR/x-$1
                  shift 2
                  mkdir -p $x $out/$name
                  if [ -d "$src" ]; then
                    x=$src
                  else
                    tar -xf "$src" -C $x --no-wildcards-match-slash --wildcards \
                      '*/COPYING*' '*/LICENSE*' '*/LICENCE*' '*/COPYRIGHT*' '*/NOTICE*' \
                      '*/LICENSES/*' "''${@/#/*/}" 2>/dev/null || true
                    # Single top-level directory.
                    local d=( $x/* ); [ ''${#d[@]} = 1 ] && [ -d "''${d[0]}" ] && x=''${d[0]}
                  fi
                  for f in $x/COPYING* $x/LICENSE* $x/LICENCE* $x/COPYRIGHT* $x/NOTICE* "''${@/#/$x/}"; do
                    [ -f "$f" ] && cp "$f" $out/$name/
                  done
                  [ -d $x/LICENSES ] && cp -r $x/LICENSES $out/$name/
                  if [ -z "$(ls -A $out/$name)" ]; then
                    echo "wheelLicenses: no license file in $name's source $src" >&2
                    exit 1
                  fi
                }
                licenses qemu-panda-engine ${engineSrc}
                mkdir -p $out/panda-ng
                cp ${./LICENSE} $out/panda-ng/LICENSE
                cp ${./REUSE.toml} $out/panda-ng/REUSE.toml
                cp -r ${./LICENSES} $out/panda-ng/LICENSES
                cp ${engineSrc}/pc-bios/README $out/qemu-panda-engine/pc-bios-README
                ${lib.concatMapStringsSep "\n" (t: "licenses ${t.name} ${t.src} ${toString t.files}") third}
              '';

          wheel =
            pkgs.runCommand "pandare2-wheel-${version}"
              {
                src = pandare2Src;
                nativeBuildInputs = [
                  pkgs.patchelf
                  (python.withPackages (ps: [
                    ps.setuptools
                    ps.wheel
                    ps.pip
                    ps.cffi
                    ps.pyelftools
                    ps.packaging
                  ]))
                ];
              }
              ''
                cp -r --no-preserve=mode $src source
                cd source/python/core
                mkdir -p ../../local_packages
                ln -s ${enginePortable}/include/panda-ng ../../local_packages/panda-ng
                echo ${version} > pandare2/version.txt
                cp ../../plugins/osi_linux/utils/kernelinfo_gdb/extract_kernelinfo.py pandare2/
                python -m pip wheel --no-deps --no-build-isolation --no-index -w $TMPDIR/pure .
                python ${./tools/wheel/repair.py} --wheel $TMPDIR/pure/pandare2-*.whl \
                  --engine ${enginePortable} --plugins ${pluginsPortable}/lib/panda/plugins \
                  --launcher ${qemuPortable}/bin/panda-qemu \
                  --arches ${lib.concatStringsSep "," arches} --licenses ${wheelLicenses} --out $out
              '';

          # Headers for building a PANDA plugin outside this tree: panda-ng's include/ and the
          # plugins' public headers, plus the engine's generated panda-ng/ headers.
          pluginSdk = pkgs.runCommand "panda-plugin-sdk-${version}" { } ''
            mkdir -p $out/include
            cp -r ${./include}/. $out/include/
            # The plugins' public headers (foo.h, foo_ppp.h, ...), not their sources.
            (cd ${./plugins} && find . \( -name '*.h' -o -name '*.hpp' \) -exec install -Dm644 {} $out/include/plugins/{} \;)
            ln -s ${engine}/include/panda-ng $out/include/panda-ng
          '';

          # Build an out-of-tree C plugin (every *.c in src) for some targets, installed as
          # lib/panda/plugins/libpanda-<pname>_<arch>-softmmu.so, the name Panda.load_plugin wants.
          buildPandaPlugin =
            {
              pname,
              src,
              targets ? [
                "x86_64"
                "aarch64"
                "riscv64"
              ],
              buildInputs ? [ ],
              extraCFlags ? "",
            }:
            pkgs.stdenv.mkDerivation {
              inherit pname version src;
              nativeBuildInputs = [ pkgs.pkg-config ];
              buildInputs = [ pkgs.glib ] ++ buildInputs;
              buildPhase = ''
                runHook preBuild
                for t in ${lib.concatStringsSep " " targets}; do
                  T=$(echo $t | tr a-z A-Z)
                  $CC -shared -fPIC -O2 -o libpanda-${pname}_$t-softmmu.so *.c \
                    -I${pluginSdk}/include -I${pluginSdk}/include/plugins \
                    -DPANDA_TGT=$t -DTARGET_$T=1 -DPLUGIN_NAME='"${pname}"' -D_GNU_SOURCE \
                    $(pkg-config --cflags --libs glib-2.0) ${extraCFlags}
                done
                runHook postBuild
              '';
              installPhase = ''
                mkdir -p $out/lib/panda/plugins
                cp *.so $out/lib/panda/plugins/
              '';
            };

          # A plugin directory with panda-ng's plugins and others (PANDA_PLUGIN_DIR / plugin_path).
          pluginDirWith =
            extra:
            "${
              pkgs.symlinkJoin {
                name = "panda-plugins-with-extra";
                paths = [ plugins ] ++ extra;
              }
            }/lib/panda/plugins";
        in
        {
          inherit
            engine
            plugins
            pluginDir
            pandare2
            mkPandare2
            pandaPython
            qemu
            qemuSystem
            dist
            deb
            wheel
            enginePortable
            mkEngine
            pluginSdk
            buildPandaPlugin
            pluginDirWith
            mkBoard
            ;
          # The patched QEMU tree the engine builds from (engine/nix/source.nix).
          engineSrc = engineSource.src;
          engineVersion = engineSource.version;
          # For working on the engine itself (engine/README.md): its build inputs, plus what the
          # series scripts and the off-cost gate use.
          engine-shell = pkgs.mkShell {
            inputsFrom = [ engine ];
            packages = with pkgs; [
              git
              jq
              bc
              xz
            ];
          };
          # For running libpanda-ng by hand against a build tree (tools/libpanda-ng/README.md):
          #   nix develop .#libpanda-ng -c bash run_one.sh <qemu-src-with-build/> <libpanda-*.so>
          # Not engine-shell: build.py runs in gdb's embedded python, which needs PYTHONPATH.
          libpanda-ng-shell = pkgs.mkShell {
            packages = with pkgs; [
              gdb
              pkg-config
              glib
            ];
            PYTHONPATH = gdbPythonPath;
          };
          plugins-shell = pkgs.mkShell (pluginTools // { NIX_CFLAGS_COMPILE = "-std=c++17"; });
          # A fast engine for iterating on the build: one target.
          engine-x86_64 = mkEngine { targets = [ "x86_64-softmmu" ]; };
        };

      perSystem = system: mkPandaNext nixpkgs.legacyPackages.${system};

      # The documentation site (mkdocs.yml, docs/). Building it first checks that the generated
      # pages are current and the feature list is true (tools/docs/gen.py --check), then builds
      # with mkdocs --strict: a broken link or a missing snippet fails.
      docsPython =
        system:
        nixpkgs.legacyPackages.${system}.python3.withPackages (ps: [
          ps.mkdocs
          ps.mkdocs-material
          ps.mkdocstrings
          ps.mkdocstrings-python
        ]);
      docsSite =
        system:
        let
          lib = nixpkgs.lib;
        in
        nixpkgs.legacyPackages.${system}.runCommand "panda-ng-docs"
          {
            src = lib.fileset.toSource {
              root = ./.;
              fileset = lib.fileset.unions [
                ./mkdocs.yml
                ./docs
                ./tools/docs
                ./tests
                ./README.md
                ./python/core
                ./include
                ./plugins
                ./flake.nix
                ./engine/base.json
                ./engine/patches
                ./engine/src/include
                ./engine/tests
                ./examples
              ];
            };
            nativeBuildInputs = [ (docsPython system) ];
          }
          ''
            cp -r $src src && chmod -R u+w src && cd src
            python3 tools/docs/gen.py --check
            mkdocs build --strict --site-dir $out
          '';
      # rust/panda-bindings/src/autogen as generate.sh makes it, from the engine's per-arch headers
      # with the flake's bindgen and rustfmt. x86_64-linux only: the headers (gcc -E on the build
      # host) and bindgen's host-ABI items (__builtin_va_list etc.) follow the build host.
      rustBindings =
        let
          pkgs = nixpkgs.legacyPackages.x86_64-linux;
          p = perSystem "x86_64-linux";
        in
        pkgs.runCommand "panda-rust-bindings"
          {
            nativeBuildInputs = [
              pkgs.rust-bindgen
              pkgs.rustfmt
              pkgs.pkg-config
            ];
            buildInputs = [ pkgs.glib ];
          }
          ''
            mkdir -p $out
            for a in ${nixpkgs.lib.concatStringsSep " " arches}; do
              bindgen ${./include}/panda.h --no-prepend-enum-name --blocklist-var IPPORT_RESERVED \
                -o $out/$a.rs -- -I${p.engine}/include -D PANDA_TGT=$a -D __STDC_FORMAT_MACROS \
                -D_GNU_SOURCE -DRUST_BINDGEN $(pkg-config --cflags glib-2.0)
            done
          '';
    in
    {
      packages = forAllSystems (
        system:
        let
          p = perSystem system;
          # tests/bench_cb.py's C baseline: a plugin that only counts callbacks.
          cbcount = p.buildPandaPlugin {
            pname = "cbcount";
            src = ./tests/plugins/cbcount;
          };
        in
        {
          inherit cbcount;
          # panda-ng's plugins plus the test plugins, for PANDA_PLUGIN_DIR (lib/panda/plugins).
          test-plugins = nixpkgs.legacyPackages.${system}.symlinkJoin {
            name = "panda-test-plugins";
            paths = [
              p.plugins
              cbcount
            ];
          };
          inherit (p)
            engine
            engine-x86_64
            plugins
            pandare2
            dist
            deb
            qemu
            wheel
            ;
          plugin-sdk = p.pluginSdk;
          engine-src = p.engineSrc;
          python = p.pandaPython;
          default = p.plugins;
          # --8<-- [start:board-example]
          # mkBoard's own check: engine/tests/modules/ext-devices.c (two devices) as a module.
          board-example = p.mkBoard {
            name = "ext-devices";
            target = "aarch64-softmmu";
            # only ext-devices.c: tests/modules also holds ext-clash.c, which clashes with pl011 on purpose
            src = nixpkgs.legacyPackages.${system}.runCommand "ext-devices-src" { } ''
              mkdir $out; cp ${./engine/tests/modules/ext-devices.c} $out/
            '';
          };
          # --8<-- [end:board-example]
          docs = docsSite system;
        }
        // p.qemuSystem
      );

      apps = forAllSystems (
        system:
        let
          p = perSystem system;
          app = drv: name: description: {
            type = "app";
            program = "${drv}/bin/${name}";
            meta = { inherit description; };
          };
        in
        nixpkgs.lib.genAttrs (map (a: "qemu-system-${a}") arches) (
          name: app p.qemu name "The PANDA engine as ${name}"
        )
        // {
          python = app p.dist "panda-python" "Python with pandare2 (the PANDA engine and plugins)";
          # `nix run .#regen-docs` from the repository root: rewrite the generated pages.
          regen-docs =
            app
              (nixpkgs.legacyPackages.${system}.writeShellScriptBin "regen-docs" ''
                exec ${docsPython system}/bin/python3 tools/docs/gen.py "$@"
              '')
              "regen-docs"
              "Regenerate docs/features.md and docs/reference from the code";
        }
        // nixpkgs.lib.optionalAttrs (system == "x86_64-linux") {
          # `nix run .#regen-rust-bindings` from the repository root.
          regen-rust-bindings =
            app
              (nixpkgs.legacyPackages.${system}.writeShellScriptBin "regen-rust-bindings" ''
                set -eu
                d=rust/panda-bindings/src/autogen
                [ -d "$d" ] || { echo "run from the panda-ng repository root" >&2; exit 1; }
                install -m 644 ${rustBindings}/*.rs "$d"/
                echo "regenerated $d"
              '')
              "regen-rust-bindings"
              "Regenerate rust/panda-bindings from the engine's headers";
        }
      );

      # pkgs.pandaNext.{engine,plugins,qemu,dist,...}, and pandare2 in every Python package set
      # (python3.withPackages (ps: [ ps.pandare2 ])), built with the consumer's nixpkgs. Use
      # `inputs.nixpkgs.follows = "panda-ng/nixpkgs"` to share this flake's builds.
      overlays.default = final: prev: {
        pandaNext = mkPandaNext final;
        pythonPackagesExtensions = prev.pythonPackagesExtensions ++ [
          (pyfinal: _pyprev: { pandare2 = final.pandaNext.mkPandare2 pyfinal; })
        ];
      };

      # lib.mkPandaNext pkgs: everything above for any nixpkgs. lib.<system>: this flake's build of
      # the engine, plugins and the helpers for building against them.
      lib = {
        inherit mkPandaNext arches defaultTargets;
      }
      // forAllSystems (
        system:
        let
          p = perSystem system;
        in
        {
          inherit (p)
            mkEngine
            mkBoard
            engine
            plugins
            pluginDir
            pluginSdk
            buildPandaPlugin
            pluginDirWith
            qemu
            pandare2
            mkPandare2
            engineSrc
            engineVersion
            ;
          python = p.pandaPython;
        }
      );

      checks = forAllSystems (
        system:
        let
          p = perSystem system;
          pkgs = nixpkgs.legacyPackages.${system};
        in
        {
          # The series applies to the pinned tarball (the strict gate with the patchedTree
          # comparison is engine/scripts/verify-series.sh, run by .github/workflows/engine.yml).
          engine-src = p.engineSrc;
          docs = docsSite system;
          # Each qemu-system-<arch> starts and lists its machines.
          qemu-system = pkgs.runCommand "panda-qemu-system-check" { } ''
            for a in ${nixpkgs.lib.concatStringsSep " " arches}; do
              ${p.qemu}/bin/qemu-system-$a -version | grep -q "QEMU emulator version"
              ${p.qemu}/bin/qemu-system-$a -machine help | grep -q .
              echo "qemu-system-$a ok"
            done
            touch $out
          '';
          # pandare2 names only what exists: every libpanda function, plugin function, C type and
          # CPUArchState field it uses resolves on every arch (tests/static/refs.py; no guest).
          # tests/static/refs-allow.txt lists the known gaps, and may only shrink.
          pandare2-refs = pkgs.runCommand "pandare2-refs-check" { } ''
            ${p.pandaPython}/bin/python3 ${./tests/static}/refs.py
            touch $out
          '';
          # More than one Panda in a process (tests/multi.py, firmware only): run() from a thread
          # lets the process exit.
          pandare2-multi = pkgs.runCommand "pandare2-multi-check" { } ''
            ${p.pandaPython}/bin/python3 ${./tests/multi.py} > log 2>&1 || { cat log; exit 1; }
            cat log
            mv log $out
          '';
          # repair.py on a synthetic engine: shipped siblings aren't bundled again, RUNPATHs keep
          # $ORIGIN, licenses and License-Expression are written (no engine build needed).
          wheel-repair =
            pkgs.runCommandCC "panda-wheel-repair-check"
              {
                nativeBuildInputs = [
                  pkgs.patchelf
                  (pkgs.python3.withPackages (ps: [
                    ps.wheel
                    ps.pyelftools
                    ps.packaging
                  ]))
                ];
              }
              ''
                cp ${./tools/wheel/repair.py} repair.py
                cp ${./tools/wheel/test_repair.py} test_repair.py
                python3 test_repair.py
                touch $out
              '';
        }
        // nixpkgs.lib.optionalAttrs (system == "x86_64-linux") {
          # The checked-in Rust bindings match the engine's headers (nix run .#regen-rust-bindings).
          rust-bindings = pkgs.runCommand "panda-rust-bindings-check" { } ''
            if ! diff -ru ${./rust/panda-bindings/src/autogen} ${rustBindings} > diff; then
              head -n 80 diff
              echo "rust/panda-bindings/src/autogen is stale: nix run .#regen-rust-bindings" >&2
              exit 1
            fi
            touch $out
          '';
        }
      );

      devShells = forAllSystems (system: {
        # The engine's toolchain: `nix develop .#engine`, then configure/ninja an
        # engine/scripts/import-series.sh tree by hand.
        engine = (perSystem system).engine-shell;
        libpanda-ng = (perSystem system).libpanda-ng-shell;
        # The plugins' toolchain without the engine: point local_packages/panda-ng at headers yourself.
        plugins = (perSystem system).plugins-shell;
      });
      formatter = forAllSystems (system: nixpkgs.legacyPackages.${system}.nixfmt-rfc-style);
    };
}
