{
  description = "A flake that consumes panda-ng: an out-of-tree plugin, a pandare2 script, plain QEMU";

  inputs = {
    panda-ng.url = "github:panda-re/panda-ng/workspace/panda-next";
    # Share panda-ng's nixpkgs, so its engine and plugins come from the same builds (and caches).
    nixpkgs.follows = "panda-ng/nixpkgs";
  };

  outputs =
    { nixpkgs, panda-ng, ... }:
    let
      forAllSystems = nixpkgs.lib.genAttrs [
        "x86_64-linux"
        "aarch64-linux"
      ];
      perSystem =
        system:
        let
          # The overlay: pkgs.pandaNext.* and ps.pandare2 in every Python package set.
          pkgs = import nixpkgs {
            inherit system;
            overlays = [ panda-ng.overlays.default ];
          };
          # The lib: panda-ng's build of the engine and the plugin helpers.
          pn = panda-ng.lib.${system};

          # An out-of-tree C plugin, built against the engine's generated headers.
          blockcount = pn.buildPandaPlugin {
            pname = "blockcount";
            src = ./plugin;
          };

          python = pkgs.python3.withPackages (ps: [ ps.pandare2 ]);

          # A pandare2 script that loads it: panda-ng's plugins plus ours in one directory.
          count-blocks = pkgs.writeShellApplication {
            name = "count-blocks";
            runtimeInputs = [ python ];
            text = ''
              export PANDA_PLUGIN_DIR=${pn.pluginDirWith [ blockcount ]}
              exec python3 ${./count.py} "$@"
            '';
          };
        in
        {
          inherit
            pkgs
            blockcount
            count-blocks
            ;
        };
    in
    {
      packages = forAllSystems (
        system:
        let
          p = perSystem system;
        in
        {
          inherit (p) blockcount count-blocks;
          # Plain QEMU from the PANDA engine, via the overlay.
          qemu = p.pkgs.pandaNext.qemu;
          default = p.count-blocks;
        }
      );

      checks = forAllSystems (
        system:
        let
          p = perSystem system;
        in
        {
          # Runs the engine (TCG, firmware only) with the plugin from Python.
          count-blocks = p.pkgs.runCommand "count-blocks-check" { } ''
            ${p.count-blocks}/bin/count-blocks x86_64 20000 | tee $out
            grep -q "^COUNT arch=x86_64" $out
          '';
          qemu-version = p.pkgs.runCommand "qemu-version-check" { } ''
            ${panda-ng.packages.${system}.qemu-system-riscv64}/bin/qemu-system-riscv64 -version | tee $out
          '';
        }
      );
    };
}
