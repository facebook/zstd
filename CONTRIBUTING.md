# Contributing to Zstandard
We want to make contributing to this project as easy and transparent as
possible.

## Our Development Process
New versions are being developed in the "dev" branch,
or in their own feature branch.
When they are deemed ready for a release, they are merged into "master".

As a consequences, all contributions must stage first through "dev"
or their own feature branch.

## Pull Requests
We actively welcome your pull requests.

1. Fork the repo and create your branch from `dev`.
2. If you've added code that should be tested, add tests.
3. If you've changed APIs, update the documentation.
4. Ensure the test suite passes.
5. Make sure your code lints.
6. If you haven't already, complete the Contributor License Agreement ("CLA").

## Contributor License Agreement ("CLA")
In order to accept your pull request, we need you to submit a CLA. You only need
to do this once to work on any of Facebook's open source projects.

Complete your CLA here: <https://code.facebook.com/cla>

## Setting up continuous integration (CI) on your fork
Zstd uses a number of different continuous integration (CI) tools to ensure that new changes
are well tested before they make it to an official release. Specifically, we use the platforms
travis-ci, circle-ci, and appveyor.

Changes cannot be merged into the main dev branch unless they pass all of our CI tests.
The easiest way to run these CI tests on your own before submitting a PR to our dev branch
is to configure your personal fork of zstd with each of the CI platforms. Below, you'll find
instructions for doing this.

### travis-ci
Follow these steps to link travis-ci with your github fork of zstd

1. Make sure you are logged into your github account
2. Go to https://travis-ci.org/
3. Click 'Sign in with Github' on the top right
4. Click 'Authorize travis-ci'
5. Click 'Activate all repositories using Github Apps'
6. Select 'Only select repositories' and select your fork of zstd from the drop down
7. Click 'Approve and Install'
8. Click 'Sign in with Github' again. This time, it will be for travis-pro (which will let you view your tests on the web dashboard)
9. Click 'Authorize travis-pro'
10. You should have travis set up on your fork now.

### circle-ci
TODO

### appveyor
Follow these steps to link circle-ci with your girhub fork of zstd

1. Make sure you are logged into your github account
2. Go to https://www.appveyor.com/
3. Click 'Sign in' on the top right
4. Select 'Github' on the left panel
5. Click 'Authorize appveyor'
6. You might be asked to select which repositories you want to give appveyor permission to. Select your fork of zstd if you're prompted
7. You should have appveyor set up on your fork now.

### General notes on CI
CI tests run every time a pull request (PR) is created or updated. The exact tests
that get run will depend on the destination branch you specify. Some tests take
longer to run than others. Currently, our CI is set up to run a short
series of tests when creating a PR to the dev branch and a longer series of tests
when creating a PR to the master branch. You can look in the configuration files
of the respective CI platform for more information on what gets run when.

Most people will just want to create a PR with the destination set to their local dev
branch of zstd. You can then find the status of the tests on the PR's page. You can also
re-run tests and cancel running tests from the PR page or from the respective CI's dashboard.

## Issues
We use GitHub issues to track public bugs. Please ensure your description is
clear and has sufficient instructions to be able to reproduce the issue.

Facebook has a [bounty program](https://www.facebook.com/whitehat/) for the safe
disclosure of security bugs. In those cases, please go through the process
outlined on that page and do not file a public issue.

## Coding Style
* 4 spaces for indentation rather than tabs

## License
By contributing to Zstandard, you agree that your contributions will be licensed
under both the [LICENSE](LICENSE) file and the [COPYING](COPYING) file in the root directory of this source tree.
