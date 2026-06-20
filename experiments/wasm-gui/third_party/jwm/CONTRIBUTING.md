Contributing to JWM
===================

Filing Issues
-------------

Issues on GitHub are very much appreciated.  These can be bug reports and
feature requests.  Don't assume that everyone knows about a bug, please
file an issue. If you don't have a GitHub account, emailing me is also
acceptable (joewing@joewing.net).

Before filing an issue, please check for duplicates. If there is a
duplicate, feel free to comment on the issue even if you have nothing
to say other than "this also affects me".  This helps prioritize.

When filing an issue, call out the version of JWM (the version, commit hash,
or snapshot number).  Although it's recommended that you try the newest
snapshot before filing an issue, sometimes that isn't easy to do, so
feel free to file the issue anyway. Most of the time, I'll know if the
issue has already been fixed.

For bug reports, please try to use a configuration file as close as possible
to the default JWM configuration.  Please describe the program or programs
affected along with their versions and provide a detailed list of steps
to reproduce the problem.  Keep in mind, that if I am unable to reproduce a
bug, it is unlikely that I will be able to fix it.  Also note that the
platform you are using (processor architecture, OS, etc.) likely will not
match mine. Screen shots can be helpful if there is a platform-specific
or closed-source program involved.

Contributing Code
-----------------

Code contributions in the form of bug fixes and features are welcome.
Please file an issue before submitting a PR and announce your intent
to address the issue yourself.  This allows discussion of the bug or
feature and prevents duplicate work.  Likewise, please announce your
intent to work on existing issues.  For issues that take longer than
a few days to address, updating the status on the issue every few days
is recommended.

JWM adheres to a fairly strict coding standard. Please take a look
around and try to mimic what is already there.

For pull requests, please squash your commits into logical changes.
Commits should have a meaningful commit message. It should be possible
to compile and run JWM at every commit.

Although I will accept patches via email, I much prefer pull requests.
A PR allows GitHub to track contributions and keeps the process in the
open.

Updating Documentation
----------------------

The documentation for JWM is available in "man page" form in the
main JWM repository as well as online at http://joewing.net/projects/jwm .
To make a correction or addition to the man page, please open a pull request.
The online documentation is stored in a separate repository (joewing/www).
Feel free to either open a pull request there too.

The documentation is currently only available in English. Unfortunately,
I lack the resources and knowledge to keep documentation in other languages
up-to-date.  There would need to be some restructuring to make other
languages work.  If you are interested in working on documentation
translations, please contact me.

As with code contributions, I recommend contacting me or filing an issue
before making larger changes to allow discussion and avoid unaccepted
changes.
