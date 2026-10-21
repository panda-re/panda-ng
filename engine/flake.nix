{
  description = "penguin-qemu: upstream QEMU release + the IGLOO patch series";

  inputs = {
    # Pinned to the exact revision penguin pins, not a channel branch, and for
    # penguin's reason rather than ours: penguin consumes this flake with
    # `inputs.nixpkgs.follows`, so the artifact it ships is ALWAYS built against
    # penguin's nixpkgs no matter what we pin here. Tracking nixos-unstable
    # therefore meant this repo's CI was green about a build nobody consumes.
    # That is not hypothetical -- the QEMU 11.1 float_status/cffi breakage was
    # invisible to every gate here and showed up the first time the artifact was
    # built the way penguin builds it, because the compiled CFFI env modules
    # must match penguin's CPython (3.13) to be loadable at all.
    # Bump this together with penguin's pin, never on its own.
    nixpkgs.url = "github:NixOS/nixpkgs/b6067cc0127d4db9c26c79e4de0513e58d0c40c9";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs =
    {
      self,
      nixpkgs,
      flake-utils,
    }:
    flake-utils.lib.eachDefaultSystem (
      system:
      let
        pkgs = import nixpkgs { inherit system; };
        bases = builtins.fromJSON (builtins.readFile ./base.json);

        # One live version, rebased forward. patches/<version>/ is keyed by
        # upstream release, so adding a second live version is additive -- but we
        # deliberately maintain exactly one.
        version = "11.1.0";
        base = bases.${version};

        sourceLib = import ./nix/source.nix { inherit pkgs; };

        src = sourceLib.qemuSource {
          patchesRoot = ./patches;
          srcRoot = ./src;
          configsRoot = ./configs;
          buildScript = ./build.sh;
          inherit version base;
        };

        # The feature set is data, and so are the libraries it needs. Deriving
        # buildInputs from the same JSON that supplies the configure flags means
        # an --enable-* can never be added without its dependency: these are
        # hard enables, so a missing library is a configure failure.
        featureCfg = builtins.fromJSON (builtins.readFile ./configs/default.json);
        nixDepNames = pkgs.lib.unique (
          pkgs.lib.concatMap (a: a.nixDeps or [ ]) featureCfg.configureArgs
        );
        extraBuildInputs = map (
          n:
          pkgs.${n} or (throw "configs/default.json names nixDep '${n}', which is not in nixpkgs")
        ) nixDepNames;

        penguin-qemu = pkgs.callPackage ./nix/qemu.nix {
          inherit src extraBuildInputs;
          version = "${base.tag}-igloo";
        };
      in
      {
        packages = {
          inherit penguin-qemu src;

          # Introspection for scripts/check-config-contract.sh: the store paths
          # of the libraries configs/default.json declares. It MUST come from
          # this flake's pinned nixpkgs -- resolving them through <nixpkgs>
          # instead compares against the channel's revision, whose store hashes
          # differ, and every dep then looks absent.
          # One line per declared dep: "<attr> <output-path>...". ALL outputs are
          # listed, because nixpkgs' default output is not always the one that
          # ends up linked: curl, bzip2 and libjpeg default to `bin` while the
          # runtime closure carries their lib output. Checking only the default
          # output reports those three as missing when they are present.
          declaredNixDeps = pkgs.writeText "declared-nix-deps" (
            pkgs.lib.concatMapStrings (
              n:
              let
                p = pkgs.${n};
                outs = if p ? all then p.all else [ p ];
              in
              "${n} ${toString (map (o: o.outPath) outs)}\n"
            ) nixDepNames
          );
          default = penguin-qemu;
          # The release artifact, exposed as a SINGLE-output derivation whose
          # $out IS the tarball file. `nix build` on the multi-output
          # `penguin-qemu.dist` would link `result` to the default `out` (the
          # unpacked tree) instead, so `cp result penguin-qemu.tar.gz` would
          # silently copy a directory. rehosting/qemu hit exactly this.
          dist = pkgs.runCommand "penguin-qemu.tar.gz" { } ''
            cp ${penguin-qemu.dist}/penguin-qemu.tar.gz "$out"
          '';
        };

        # `nix flake check` runs the series gate: the patches must apply to the
        # pristine upstream tarball and produce base.json's recorded tree.
        checks.series = pkgs.runCommand "series-applies" { } ''
          test -d ${src}
          test -f ${src}/system/penguin.c
          test -f ${src}/include/system/penguin.h
          test -f ${src}/configs/default.json
          test -x ${src}/build.sh
          touch $out
        '';

        devShells.default = pkgs.mkShell {
          packages = with pkgs; [
            git
            python3
            jq
          ];
        };
      }
    );
}
