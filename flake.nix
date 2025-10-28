{
  description = "PANDA: Platform for Architecture-Neutral Dynamic Analysis";
  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs?ref=nixos-unstable";
    panda-qemu-src = {
      url = "github:panda-re/qemu?ref=wrapup-rebase";
      flake = false;
    };
    libpanda-ng-src = {
      url = "github:panda-re/libpanda-ng";
      flake = false;
    };
  };
  outputs =
    {
      nixpkgs,
      panda-qemu-src,
      libpanda-ng-src,
      ...
    }:
    let
      forAllSystems = nixpkgs.lib.genAttrs nixpkgs.lib.systems.flakeExposed;
      version = "main";
      perSystem = forAllSystems (
        system:
        let
          pkgs = nixpkgs.legacyPackages.${system};
          libExt = if pkgs.stdenv.isDarwin then "dylib" else "so";
          pyPkgs = pkgs.python3Packages;
          targetList = [
            "x86_64-softmmu"
            "i386-softmmu"
            "arm-softmmu"
            "aarch64-softmmu"
            "ppc-softmmu"
            "mips-softmmu"
            "mipsel-softmmu"
            "mips64-softmmu"
            "ppc64-softmmu"
            "mips64el-softmmu"
            "loongarch64-softmmu"
            "riscv32-softmmu"
            "riscv64-softmmu"
          ];
          # TODO remove this patch once wrapup-rebase branch is rebased again
          libpandaPatch = pkgs.fetchpatch {
            url = "https://patch-diff.githubusercontent.com/raw/panda-re/libpanda-ng/pull/3.diff";
            hash = "sha256-FN1OUdRhOIK+meOlJH9kPysjrM50qYQVnl/Ll6zGF6w=";
            revert = true;
          };
          qemuSubprojects = pkgs.stdenv.mkDerivation {
            name = "qemu-subprojects";
            inherit version;
            src = panda-qemu-src;
            nativeBuildInputs = with pkgs; [
              meson
              git
              cacert
            ];
            buildCommand = ''
              mkdir -pv work
              cp -r --no-preserve=mode $src/meson.build work/
              cp -r --no-preserve=mode $src/meson_options.txt work/
              cp -r --no-preserve=mode $src/subprojects work/subprojects

              pushd work
              meson subprojects download
              find subprojects -type d -name .git -prune -execdir rm -r {} +
              popd

              cp -r work/subprojects $out
            '';
            outputHash = "sha256-eUw7yBWxRKJbfhKvZDRNpTSaxrnDYr31Tkx35Myx4Fs=";
            outputHashAlgo = "sha256";
            outputHashMode = "recursive";
          };
          qemu = pkgs.qemu.overrideAttrs (old: {
            inherit version;
            src = panda-qemu-src;
            nativeBuildInputs =
              (old.nativeBuildInputs or [ ])
              ++ (with pkgs; [
                pyPkgs.python
                pyPkgs.cffi
                pyPkgs.tree-sitter
                pyPkgs.tree-sitter-grammars.tree-sitter-c
                gdb
              ]);
            configureFlags = [
              "--enable-plugins"
              "--disable-containers"
              "--target-list=${builtins.concatStringsSep "," targetList}"
            ];
            postUnpack = ''
              cp -r --no-preserve=mode ${qemuSubprojects}/. ./source/subprojects/
              cp -r --no-preserve=mode ${libpanda-ng-src} ./libpanda-ng
              patch --verbose -d libpanda-ng -p0 --unified --strip 1 < ${libpandaPatch}
              patchShebangs ./source/scripts
            '';
            postBuild =
              if pkgs.stdenv.isLinux then
                ''
                  pushd $TMPDIR/libpanda-ng
                  bash run_all.sh ../source
                  popd
                ''
              else
                "";
            postInstall = ''
              cp -v ./contrib/plugins/libpanda_plugin_interface.${libExt} $out/lib/
            ''
            + (
              if pkgs.stdenv.isLinux then
                ''
                  cp $TMPDIR/libpanda-ng/*.h $out/include/
                ''
              else
                ""
            );
          });
          panda = pkgs.stdenv.mkDerivation (finalAttrs: {
            name = "panda";
            inherit version;
            src = ./.;
            nativeBuildInputs = with pkgs; [
              pkg-config
              meson
              ninja
              cargo
            ];
            buildInputs = with pkgs; [
              glib
              curl
              pyPkgs.python
              pyPkgs.pycparser
            ];
            NIX_CFLAGS_COMPILE = "-std=c++17";
            mesonFlags = [
              (pkgs.lib.strings.mesonOption "targets" (builtins.concatStringsSep "," targetList))
            ];
            preConfigure = ''
              mkdir -p local_packages
              ln -s ${perSystem.x86_64-linux.qemu}/include local_packages/panda-ng
            '';
            preInstall = ''
              cp plugins/*/*.h plugins/
              for dir in plugins/rust/rust_skeleton/*; do
                arch=$(basename $dir)
                cp -r $dir plugins/rust_rust_skeleton_$arch
              done
            '';
            postInstall = ''
              mkdir -pv $out/lib/panda/panda
              mv $out/plugin $out/lib/panda/panda/plugins
            '';
          });
          pypandaBuilder =
            ps:
            ps.buildPythonPackage {
              pname = "pandare2";
              inherit version;
              format = "setuptools";
              src = ./python/core;
              propagatedBuildInputs = with ps; [
                cffi
                colorama
              ];
              preBuild = ''
                substituteInPlace setup.py --replace-fail /usr/include/panda-ng ${perSystem.x86_64-linux.qemu}/include
                grep 'plugin_dir =' setup.py
                sed -i 's,plugin_dir = .*,plugin_dir="${panda.src}/plugins",' setup.py
              '';
              postInstall = ''
                mkdir -pv $out/lib/qemu/build
                cp -R ${qemu}/lib/libpanda-*.${libExt} $out/lib/qemu/build/
                mkdir -pv $out/lib/qemu/build/contrib/plugins
                cp -R ${qemu}/lib/libpanda_plugin_interface.${libExt} $out/lib/qemu/build/contrib/plugins/
                mkdir -pv $out/lib/qemu/pc-bios
                cp -R ${qemu}/share/qemu/*.rom ${qemu}/share/qemu/*.bin $out/lib/qemu/pc-bios/
              '';
            };
          pypanda = pypandaBuilder pyPkgs;
          pythonWithPypanda = pyPkgs.python.withPackages (ps: [ pypanda ]);
        in
        {
          inherit
            qemu
            qemuSubprojects
            panda
            pypanda
            pythonWithPypanda
            pypandaBuilder
            ;
        }
      );
    in
    {
      packages = forAllSystems (system: {
        inherit (perSystem.${system})
          qemu
          qemuSubprojects
          panda
          pypanda
          pythonWithPypanda
          ;
      });
      lib = forAllSystems (system: {
        inherit (perSystem.${system}) pypandaBuilder;
      });
      formatter = forAllSystems (system: nixpkgs.legacyPackages.${system}.nixfmt-rfc-style);
    };
}
