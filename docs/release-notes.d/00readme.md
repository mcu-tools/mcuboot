# Pending release notes directory

This directory contains release note entries that have not been merged
into the main release-notes.md document.

Generally a release note entry should be created for changes that:

- Fix bugs in the code.
- Implement new features.
- Change existing behavior.

Release notes are generally not needed for:

- Some documentation improvements.
- Strictly internal changes to the code that won't be visible to users
  of the code.

## Release note format

Release notes are included in files under this `docs/release-notes.d`
directory and have a name of `*.md`.  They will be included in the
`release-notes.md` file, and should be formatted as a Markdown list
entry.  (A script will be developed to collect these, ordered by when
the commits were added to the tree.)

Choose a filename that is related to what this change does.  The names
are not used for anything in particular, but to keep the files
distinct so that there isn't a concern with merge conflicts as
different pull requests merge in different orders.
