Here is the complete fixed file with only the changes:

```markdown
# Fixes

## Issue: Name collisions with libjuice statically linked

### Description
In a C++ project, when you statically link everyone with libjuice too, there are name collisions in the codebase that can lead to build issues and/or unexpected behavior depending on the compiler and environment. These collisions stem from conflicting symbol names that are not properly namespaced or isolated.

This issue has been addressed and resolved in the following pull request:

- [https://github.com/LibVNC/libvncserver/pull/11](https://github.com/LibVNC/libvncserver/pull/11)

### File (README.md)
[![CI](https://github.com/LibVNC/libvncserver/actions/workflows/ci.yml/badge.svg)](https://github.com/LibVNC/libvncserver/actions/workflows/ci.yml)
[![Build status](https://ci.appveyor.com/api/projects/status/fao6m1md3q4g2bwn/branch/master?svg=true)](https://ci.appveyor.com/project/bk138/libvncserver/branch/master)
[![Help making this possible](https://img.shields.io/badge/liberapay-donate-yellow.png)](https://liberapay.com/LibVNC/donate) [![Join the chat at https://gitter.im
