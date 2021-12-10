# Patch submission

The development of MCUboot takes place in the [MCUboot GitHub
repository](https://github.com/mcu-tools/mcuboot).

To submit patches, use GitHub pull requests.

Each commit has to have, in the commit message, a "Signed-off-by" line
that mentions the author (and the committer, if that is different). You
must add this line at the end of the commit text, separated by a blank
line. You can also add a line linking the commit to a specific GitHub
issue, as this section supports multiple lines, similarly to RFC-2822.

The supported trailer lines are structured as follows:

- A line that indicates that the signer agrees to the "Developer
Certificate of Origin" located at the bottom of this page:

  ```
      Signed-off-by: Developer Name <devname@example.com>
  ```

- A line that links this commit to specific GitHub issues, if present:

  ```
  Keyword #GH_issue_number
  ```

  For more details about linking a GitHub pull request to a GitHub issue,
  see this [link]
  (https://docs.github.com/en/issues/tracking-your-work-with-issues/linking-a-pull-request-to-an-issue).

## Developer certificate of origin

The following is the "Developer Certificate of Origin":

```
Developer Certificate of Origin
Version 1.1

Copyright (C) 2004, 2006 The Linux Foundation and its contributors.
1 Letterman Drive
Suite D4700
San Francisco, CA, 94129

Everyone is permitted to copy and distribute verbatim copies of this
license document, but changing it is not allowed.


Developer's Certificate of Origin 1.1

By making a contribution to this project, I certify that:

(a) The contribution was created in whole or in part by me and I
    have the right to submit it under the open source license
    indicated in the file; or

(b) The contribution is based upon previous work that, to the best
    of my knowledge, is covered under an appropriate open source
    license and I have the right under that license to submit that
    work with modifications, whether created in whole or in part
    by me, under the same open source license (unless I am
    permitted to submit under a different license), as indicated
    in the file; or

(c) The contribution was provided directly to me by some other
    person who certified (a), (b) or (c) and I have not modified
    it.

(d) I understand and agree that this project and the contribution
    are public and that a record of the contribution (including all
    personal information I submit with it, including my sign-off) is
    maintained indefinitely and may be redistributed consistent with
    this project or the open source license(s) involved.
```
