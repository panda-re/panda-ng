{
  description = "panda-next: the PANDA engine (qemu_builder + --enable-panda), its generated headers, and plugins";

  inputs = {
    # The engine: qemu_builder's patched 11.1 source (series + vendored src/, PANDA included).
    # Local development: --override-input qemu-builder path:/path/to/qemu_builder
    qemu-builder.url = "git+https://github.com/rehosting/qemu_builder?ref=workspace/panda-next-pandang";
    # One nixpkgs for everything, the one qemu_builder (and penguin) pin.
    nixpkgs.follows = "qemu-builder/nixpkgs";
    libpanda-ng-src = {
      url = "git+https://github.com/panda-re/libpanda-ng";
      flake = false;
    };
  };

  outputs =
    {
      self,
      nixpkgs,
      qemu-builder,
      libpanda-ng-src,
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

          # libpanda-ng with local fixes until they land upstream.
          libpanda-ng = pkgs.applyPatches {
            name = "libpanda-ng";
            src = libpanda-ng-src;
            patches = [ ./nix/libpanda-ng-fixes.patch ];
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
              src = qemu-builder.packages.${system}.src;

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
                # build.py preprocesses through /tmp/simple.{c,out}: concurrent engine builds
                # (no sandbox in the VM) overwrite each other's and one target's header comes out
                # wrong. Keep it in this build's directory.
                sed -i "s|/tmp/simple|$NIX_BUILD_TOP/simple|g" ../libpanda-ng/build.py
                (cd ../libpanda-ng && PYTHONPATH=${gdbPythonPath} bash run_all.sh $PWD/../source)
                runHook postBuild
              '';

              installPhase = ''
                runHook preInstall
                ninja -C build install
                cp build/contrib/plugins/libpanda_plugin_interface.so $out/lib/
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

          # The Python users run PANDA from: pandare2 and its optional plog reader (protobuf).
          pandaPython = python.withPackages (ps: [
            pandare2
            ps.protobuf
          ]);

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
            .${system} or "unsupported";
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
                  --arches ${lib.concatStringsSep "," arches} --out $out
              '';

          # Headers for building a PANDA plugin outside this tree: panda-ng's include/ and the
          # plugins' public headers, plus the engine's generated panda-ng/ headers.
          pluginSdk = pkgs.runCommand "panda-plugin-sdk-${version}" { } ''
            mkdir -p $out/include
            cp -r ${./include}/. $out/include/
            cp -r ${./plugins} $out/include/plugins
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
            ;
          # For running libpanda-ng by hand against a build tree:
          #   nix develop .#libpanda-ng -c bash run_one.sh <qemu-src-with-build/> <libpanda-*.so>
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
    in
    {
      packages = forAllSystems (
        system:
        let
          p = perSystem system;
        in
        {
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
          python = p.pandaPython;
          default = p.plugins;
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
            engine
            plugins
            pluginDir
            pluginSdk
            buildPandaPlugin
            pluginDirWith
            qemu
            pandare2
            mkPandare2
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
          # Each qemu-system-<arch> starts and lists its machines.
          qemu-system = pkgs.runCommand "panda-qemu-system-check" { } ''
            for a in ${nixpkgs.lib.concatStringsSep " " arches}; do
              ${p.qemu}/bin/qemu-system-$a -version | grep -q "QEMU emulator version"
              ${p.qemu}/bin/qemu-system-$a -machine help | grep -q .
              echo "qemu-system-$a ok"
            done
            touch $out
          '';
        }
      );

      devShells = forAllSystems (system: {
        libpanda-ng = (perSystem system).libpanda-ng-shell;
        # The plugins' toolchain without the engine: point local_packages/panda-ng at headers yourself.
        plugins = (perSystem system).plugins-shell;
      });
      formatter = forAllSystems (system: nixpkgs.legacyPackages.${system}.nixfmt-rfc-style);
    };
}
