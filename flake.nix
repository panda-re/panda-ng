{
  description = "PANDA: Platform for Architecture-Neutral Dynamic Analysis";
  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs?ref=nixos-unstable";
    panda-qemu-src = {
      url = "github:panda-re/qemu";
      flake = false;
    };
    libpanda-ng-src = {
      # TODO update org and ref once libpanda-ng PR is merged
      url = "github:rehostingdev/libpanda-ng?ref=fixes";
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
            outputHash = "sha256-3trLfYIhSxKy72DlDZ7PluCpx9/UBcwY8a9LV3udYkg=";
            outputHashAlgo = "sha256";
            outputHashMode = "recursive";
          };
          qemu = pkgs.stdenv.mkDerivation (finalAttrs: {
            name = "qemu";
            inherit version;
            src = panda-qemu-src;
            inherit qemuSubprojects libpanda-ng-src;
            enableParallelBuilding = true;
            nativeBuildInputs = with pkgs; [
              pyPkgs.python
              pyPkgs.cffi
              pyPkgs.tree-sitter
              pyPkgs.tree-sitter-grammars.tree-sitter-c
              pkg-config
              ninja
              gdb
              autoPatchelfHook
            ];
            buildInputs = with pkgs; [
              pyPkgs.python
              glib
              dtc
            ];
            postUnpack = ''
              cp -r --no-preserve=mode ${finalAttrs.qemuSubprojects}/. ./source/subprojects/
              cp -r --no-preserve=mode ${finalAttrs.libpanda-ng-src} ./libpanda-ng
              patchShebangs ./source/scripts
            '';
            preConfigure = ''
              mkdir -pv build
              cd build
            '';
            configureScript = "../configure";
            configureFlags = [
              "--enable-plugins"
              "--disable-containers"
              "--target-list=${builtins.concatStringsSep "," targetList}"
            ];
            postBuild = ''
              pushd $TMPDIR/libpanda-ng
              bash run_all.sh ../source
              popd
            '';
            postInstall = ''
              cp $TMPDIR/libpanda-ng/*.h $out/include/
              cp -v ./contrib/plugins/libpanda_plugin_interface.so $out/lib/
            '';
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
              ln -s ${qemu}/include local_packages/panda-ng
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
            qemu:
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
                substituteInPlace setup.py --replace-fail /usr/include/panda-ng ${qemu}/include
                grep 'plugin_dir =' setup.py
                sed -i 's,plugin_dir = .*,plugin_dir="${panda.src}/plugins",' setup.py
              '';
              postInstall = ''
                mkdir -pv $out/lib/qemu/build
                cp -R ${qemu}/lib/libpanda-*.so $out/lib/qemu/build/
                mkdir -pv $out/lib/qemu/build/contrib/plugins
                cp -R ${qemu}/lib/libpanda_plugin_interface.so $out/lib/qemu/build/contrib/plugins/
                mkdir -pv $out/lib/qemu/pc-bios
                cp -R ${qemu}/share/qemu/*.rom ${qemu}/share/qemu/*.bin $out/lib/qemu/pc-bios/
              '';
            };
          pypanda = pypandaBuilder pyPkgs qemu;
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
