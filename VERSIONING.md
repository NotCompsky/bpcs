# Summary

Follows an extended version of [semantic versioning 2.0.0](https://semver.org/). Since data compatibility is paramount, the method version is prepended to the standard semver version string.

Given a version number METHOD.MAJOR.MINOR.PATCH or METHOD.MAJOR.MINOR.PATCH-devN.COMMIT:

| Number | Represents |
| - | - |
| METHOD | Method version. All versions with the same method version can perfectly understand one another's output. |
| MAJOR | API version. |
| MINOR | An incremental update that adds functionality or performance without breaking existing API. |
| PATCH | An update that only fixes a bug. |
| dev | Only present on dev builds. Tags build as experimental build leading to the release MAJOR.MINOR.PATCH build. |
| N | Only present on dev builds. Describes what is being worked on: features, bugs, documentation for 0, 1, 2 respectively. |
| COMMIT | Only present on dev builds. Represents the number of commits progressed along the experimental build towards the release build. |

# SPECIAL CASES

Builds with MAJOR version 0 are initial dev builds, created before versioning was standardised, and follow a different versioning - MINOR versions represent API backwards-incompatible changes.

# Examples

* 1.1.1.1 represents the first bug fix release of 1.1.1.0. Illegal unless 1.1.1.0 version exists.
* 1.1.0.0-dev0.0 represents the initial development build towards 1.1.0.0
* 1.1.1.1-dev0.0 represents the initial development build towards 1.1.1.1. Illegal unless 1.1.1.0 version exists.
* 1.1.1.1-dev0.10 represents the build 10 commits along from 1.1.1.1-dev0.0
