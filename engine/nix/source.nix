# The engine's source: the pristine upstream QEMU release tarball (pinned by hash in base.json),
# the patch series patches/<version>/series applied in order, and the overlay src/ (files we
# authored outright) copied on top. From qemu_builder's nix/source.nix; panda-ng overlays src/
# only (qemu_builder also copied in penguin's configs/ and build.sh, which panda-ng doesn't use).
#
#   engineSource = import ./engine/nix/source.nix { inherit pkgs; };
#   engineSource.src            # the patched tree (a store path), what the engine builds from
#   engineSource.version        # the upstream release, e.g. "11.1.2" (base.json's one key)
#
# Two properties of QEMU release tarballs this relies on (verified against v11.1.0 and v11.1.2):
# QEMU's .gitattributes has no export-ignore, so every file the series touches is identical in the
# tag and the tarball; and the tarball ships the meson subprojects (keycodemapdb,
# berkeley-softfloat-3, berkeley-testfloat-3) already populated, so nothing else is fetched.
{ pkgs }:

let
  inherit (pkgs) lib;
  root = ../.;
  bases = builtins.fromJSON (builtins.readFile (root + "/base.json"));
  # One live upstream version, rebased forward: base.json has exactly one entry.
  versions = builtins.attrNames bases;
  version =
    if builtins.length versions == 1 then
      builtins.head versions
    else
      throw "engine/base.json must have exactly one version, has: ${toString versions}";
  base = bases.${version};

  # The series file, in order, ignoring blank lines and # comments (quilt convention). Each
  # entry is <version>/<name>.patch, relative to patches/.
  readSeries =
    patchesRoot: version:
    let
      # Read through the path, not a store copy, so evaluation needs no store write
      # (`nix flake check --no-build`); the patches themselves are copied as before.
      lines = lib.splitString "\n" (builtins.readFile (patchesRoot + "/${version}/series"));
      keep = l: l != "" && !(lib.hasPrefix "#" l);
    in
    map (l: "${patchesRoot}/${l}") (builtins.filter keep (map lib.trim lines));
in
{
  inherit version base readSeries;

  src = pkgs.applyPatches {
    name = "qemu-${base.tag}-panda";
    src = pkgs.fetchurl {
      url = base.url or "https://download.qemu.org/qemu-${base.tag}.tar.xz";
      inherit (base) hash;
    };
    patches = readSeries (root + "/patches") version;
    # The overlay: files we authored, copied rather than carried as add-file patches, which keeps
    # the invariant that no patch in the series references a file we created.
    postPatch = ''
      cp -r ${root + "/src"}/. .
    '';
  };
}
