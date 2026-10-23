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

      perSystem =
        system:
        let
          pkgs = nixpkgs.legacyPackages.${system};
          lib = pkgs.lib;

          # libpanda-ng with local fixes until they land upstream.
          libpanda-ng = pkgs.applyPatches {
            name = "libpanda-ng";
            src = libpanda-ng-src;
            patches = [ ./nix/libpanda-ng-fixes.patch ];
            # build.py preprocessed through a fixed /tmp/simple.c: two engine builds at once (the
            # VM's nix builds unsandboxed) overwrote each other's and generated a broken header.
            postPatch = ''
              substituteInPlace build.py \
                --replace-fail 'from os.path import join, realpath' 'import os
              from os.path import join, realpath' \
                --replace-fail 'open("/tmp/simple.c","w")' 'open(SIMPLE + ".c","w")' \
                --replace-fail '/tmp/simple.c -o /tmp/simple.out' '{SIMPLE}.c -o {SIMPLE}.out' \
                --replace-fail 'open("/tmp/simple.out")' 'open(SIMPLE + ".out")'
              sed -i '/^import os$/a SIMPLE = os.path.join(os.environ.get("TMPDIR", "/tmp"), "simple-%d" % os.getpid())' build.py
            '';
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

          # The engine. Keeps nothing of the build tree but what it installs, plus the headers
          # libpanda-ng generates from it (which need the tree, so they are made here).
          mkEngine =
            {
              targets ? defaultTargets,
            }:
            pkgs.stdenv.mkDerivation {
              pname = "panda-engine";
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
                  --disable-virglrenderer --disable-spice --disable-vnc --disable-containers
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

          # panda-ng's plugins, built against the engine's generated headers.
          plugins = pkgs.stdenv.mkDerivation {
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

          python = pkgs.python3;
          pluginDir = "${plugins}/lib/panda/plugins";

          # pandare2, with this engine and these plugins ahead of its /usr/local defaults.
          pandare2 = python.pkgs.buildPythonPackage {
            pname = "pandare2";
            inherit version;
            # setup.py reads python/core and the plugins' PPP headers.
            src = lib.fileset.toSource {
              root = ./.;
              fileset = lib.fileset.unions [
                ./python/core
                ./plugins
              ];
            };
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
                --replace-fail 'pot_paths = [system_build,' 'pot_paths = ["${engine}/lib", system_build,'
              substituteInPlace pandare2/panda.py \
                --replace-fail 'realpath(pjoin(__file__, "../../../../build/plugins")),' \
                  '"${pluginDir}", realpath(pjoin(__file__, "../../../../build/plugins")),' \
                --replace-fail 'f"/usr/local/lib/panda/{self.arch_name}"' '"${pluginDir}"' \
                --replace-fail 'realpath(pjoin(self.panda, f"../contrib/plugins/libpanda_plugin_interface.{self.dll_extension}"))' \
                  '"${engine}/lib/libpanda_plugin_interface.so"' \
                --replace-fail '"/usr/local/share/panda",' '"${engine}/share/qemu", "/usr/local/share/panda",'
            '';
            build-system = [
              python.pkgs.setuptools
              python.pkgs.cffi
            ];
            dependencies = [ python.pkgs.cffi ];
            pythonImportsCheck = [ "pandare2" ];
          };

          # The Python users run PANDA from: pandare2 and its optional plog reader (protobuf).
          pandaPython = python.withPackages (ps: [
            pandare2
            ps.protobuf
          ]);

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
            .${system};
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
        in
        {
          inherit
            engine
            plugins
            pandare2
            pandaPython
            dist
            deb
            mkEngine
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
            ;
          python = p.pandaPython;
          default = p.plugins;
        }
      );
      devShells = forAllSystems (system: {
        libpanda-ng = (perSystem system).libpanda-ng-shell;
        # The plugins' toolchain without the engine: point local_packages/panda-ng at headers yourself.
        plugins = (perSystem system).plugins-shell;
      });
      lib = forAllSystems (system: { inherit (perSystem system) mkEngine; });
      formatter = forAllSystems (system: nixpkgs.legacyPackages.${system}.nixfmt-rfc-style);
    };
}
