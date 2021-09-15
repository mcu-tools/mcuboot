# Release process

This page describes the release process used with MCUboot.

## Version numbering

MCUboot uses [semantic versioning][semver], where version numbers follow a
`MAJOR.MINOR.PATCH` format with the following guidelines on incrementing
the numbers:

1. MAJOR version when there are incompatible API changes.
2. MINOR version when new functionalities were added in a
backward-compatible manner.
3. PATCH version when there are backward-compatible bug fixes.

We add pre-release tags using the format `MAJOR.MINOR.PATCH-rc1`.

In the documentation, an MCUBoot development version is marked using the
format `MAJOR.MINOR.PATCH-dev`.

## Release notes

Before making a release, update the `docs/release-notes.md` file to
describe the release. This should be a high-level description of the
changes, not a list of the git commits.

## Release candidates

Before each release, tags are made (see below) for at least one release
candidate (a.b.c-rc1, followed by a.b.c-rc2, followed by the official
a.b.c release). The intent is to freeze the code and allow testing.

During the time between the rc1 and the final release, the only changes
that should be merged into the master branch are those to fix bugs found
in the rc and Mynewt metadata, as described in the next section.

## imgtool release

imgtool is released through pypi.org (The Python package index).

It requires that its version is updated by editing
`scripts/imgtool/__init__.py` and modifying the exported version:

```
imgtool_version = "A.B.CrcN"
```

This version should match the current release number of MCUboot. The
suffix `rcN` (with no dash) is accepted only for the pre-release versions
under test, while numbers are accepted only for the final releases.

For more information, see [this
link](https://www.python.org/dev/peps/pep-0440/#pre-releases).

## Mynewt release information

On Mynewt, `newt` always fetches a versioned MCUBoot release, so after the
rc step is finished, the release needs to be exported by modifying
`repository.yml` in the root directory; it must be updated with the new
release version, including updates to the pseudo keys (`*-(latest|dev)`).

## Tagging and release

To make a release, make sure your local repo is on the tip version by
fetching from origin. Typically, the releaser should create a branch named
after the particular release.

Create a commit on top of the branch that modifies the version number in
the top-level `README.md`, and create a commit, with just this change,
with a commit text similar to "Bump to version a.b.c". Having the version
bump helps to make the releases easier to find, as each release has a
commit associated with it, and not just a tag pointing to another commit.

Once this is done, the release should create a signed tag with the
appropriate tag name:

``` bash
git tag -s va.b.c-rcn
```

The releaser will need to make sure that git is configured to use the
proper signing key, and that the public key is signed by enough parties to
be trusted.

At this point, the tag can be pushed to GitHub to make the actual release
happen:

``` bash
git push origin HEAD:refs/heads/main
git push origin va.b.c-rcn
```

## Branching after a release

After the final (non-rc) a.b.0 release is made, a new branch must be
created and pushed:

``` bash
git checkout va.b.c
git checkout -b va.b-branch
git push origin va.b-branch
```

This branch will be used to generate new incremental `PATCH` releases for
bug fixes or required minor updates (eg, new `imgtool` features).

## Post-release actions

Mark the MCUBoot version as a development version.

The version number used should be specified for the next expected release.
It should be larger than the last release version by incrementing the
MAJOR or the MINOR number. It is not necessary to define the next version
precisely as the next release version might still be different as it might
be needed to do:

- a patch release
- a MINOR release while a MAJOR release was expected
- a MAJOR release while a MINOR release was expected

[semver]: http://semver.org/
