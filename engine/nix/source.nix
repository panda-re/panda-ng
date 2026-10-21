# QEMU source = PRISTINE UPSTREAM RELEASE TARBALL + the IGLOO patch series
#                + the vendored Penguin sources.
#
# Mirrors linux_builder/nix/source.nix. The upstream base is a real release
# tarball pinned by hash in base.json, never a fork branch.
#
# Two properties of QEMU release tarballs make this cleaner here than it was for
# the kernel, both verified against v11.1.0:
#
#   * QEMU's .gitattributes has NO export-ignore, so there is no subtractive
#     tag-vs-tarball skew to whitelist. linux_builder had to allow four paths;
#     we allow none. Every file the series touches is byte-identical between the
#     v11.1.0 tag and the v11.1.0 tarball.
#
#   * The tarball ships the meson git-wrap subprojects already populated
#     (keycodemapdb, berkeley-softfloat-3, berkeley-testfloat-3). Building from
#     a git base required vendoring all three via fetchFromGitLab with
#     hand-maintained per-version hashes; from a tarball that entire block
#     disappears. slirp and dtc remain satisfied by system libraries.
{ pkgs }:

let
  inherit (pkgs) lib;

  # Read a series file into an ordered list of patch paths, ignoring blank lines
  # and # comments (quilt convention).
  readSeries =
    patchesRoot: version:
    let
      lines = lib.splitString "\n" (builtins.readFile "${patchesRoot}/${version}/series");
      keep = l: l != "" && !(lib.hasPrefix "#" l);
    in
    map (l: "${patchesRoot}/${l}") (builtins.filter keep (map lib.trim lines));

in
{
  inherit readSeries;

  # base: { tag = "11.1.0"; url = "..."; hash = "sha256-..."; }
  qemuSource =
    {
      patchesRoot,
      srcRoot,
      configsRoot,
      buildScript,
      version,
      base,
    }:
    pkgs.applyPatches {
      name = "qemu-${base.tag}-igloo";
      src = pkgs.fetchurl {
        url = base.url or "https://download.qemu.org/qemu-${base.tag}.tar.xz";
        inherit (base) hash;
      };
      patches = readSeries patchesRoot version;

      # The vendored half of the delta. These are files we authored outright, so
      # they are copied in rather than carried as add-file patches -- which is
      # what keeps the invariant that no patch in the series ever references a
      # file we created.
      postPatch = ''
        cp -r ${srcRoot}/. .
        mkdir -p configs
        cp -r ${configsRoot}/. configs/
        install -m 0755 ${buildScript} build.sh
      '';
    };
}
