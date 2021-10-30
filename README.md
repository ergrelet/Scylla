Scylla - x64/x86 Imports Reconstruction
=======================================

This repository contains a fork of [NtQuery/Scylla](https://github.com/NtQuery/Scylla)
with tweaks from [x64dbg/Scylla](https://github.com/x64dbg/Scylla) as well as
additional fixes, refactoring and bindings.

Introduction
------------

ImpREC, CHimpREC, Imports Fixer... this are all great tools to rebuild an import table, 
but they all have some major disadvantages, so I decided to create my own tool for this job.

Scylla's key benefits are:

 - x64 and x86 support
 - full unicode support
 - written in C/C++
 - plugin support
 - works great with Windows 7

This tool was designed to be used with Windows 7 x64, so it is recommended to use this operating system.

License
-------

Source code is licensed under GNU GENERAL PUBLIC LICENSE v3.0.

Known Bugs
----------

### Windows 7 x64

Sometimes the API kernel32.dll GetProcAddress cannot be resolved, because the IAT has an entry from apphelp.dll
Solution? I don't know

### ImpREC plugin support:

Some ImpREC Plugins don't work with Windows Vista/7 because they don't "return 1" in the DllMain function.

Keyboard Shortcuts
------------------

- CTRL + D: [D]ump
- CTRL + F: [F]ix Dump
- CTRL + R: PE [R]ebuild
- CTRL + O: L[o]ad Tree
- CTRL + S: [S]ave Tree
- CTRL + T: Auto[t]race
- CTRL + G: [G]et Imports
- CTRL + I: [I]AT Autosearch
