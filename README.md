[![Codacy Badge](https://api.codacy.com/project/badge/Grade/5e07db76c668493d888a7f9b97d79821)](https://app.codacy.com/gh/israpps/wLaunchELF_ISR?utm_source=github.com&utm_medium=referral&utm_content=israpps/wLaunchELF_ISR&utm_campaign=Badge_Grade_Settings)
[![Automated-Build](https://github.com/israpps/wLaunchELF_ISR/actions/workflows/compile.yml/badge.svg)](https://github.com/israpps/wLaunchELF_ISR/actions/workflows/compile.yml)
![GitHub all releases](https://img.shields.io/github/downloads/israpps/wLaunchELF_ISR/total?logo=github)
# __About this mod__
this uLaunchELF mod was my first PS2 project.

It features:

- Timestamp manipulation feature to fix the date of any memory card folder containing any icon-based exploit _(\*tuna)_
- Extra file extensions for Text Editor ShortCuts
- ~`100kb` smaller that it´s original counterpart (wLE 41e4ebe) (this was possible thanks to CI with ps2dev:v1.0 toolchain)
- Support for PS3/PS4 Dualshocks thanks to Alex Parrado
#### this mod has proven to be excellent for HDD, USB and MC management.

> this mod is already bundled on any mod/project/repack made by me (and it´s auto-updated if that project is hosted here on github)

### DOWNLOAD:
- [Normal version](https://github.com/israpps/wLaunchELF_ISR/releases/tag/latest)
- [Hardcoded Spanish](https://github.com/israpps/wLaunchELF_ISR/releases/tag/espa%C3%B1ol)

### Explanation of download filenames

> release filename changes according to the included features:

- `BOOT`: base filename, means nothing
- `UNC`: executable is Uncompressed
- `EE_SIO`: supports printing debug information via the EmotionEngine serial port (useless for users)
- `NO_NETWORK`: network features are disabled and network IRX drivers stripped away, with the purpose of making a smaller wLaunchELF for users who don't use network
- `NO_IOP_RESET`: build of wLaunchELF wich does not reset the I/O CPU upon startup, __DON'T USE IF YOU DON'T KNOW WHAT IT MEANS__
- `DVRP`: support for accessing encrypted portion of the PSX-DESR Internal HDD memory (in filebrowser represented as `dvr_hdd0:`)

# **original readme**
wLaunchELF, formerly known as uLaunchELF, also known as wLE or uLE (abbreviated), is an open source file manager and executable launcher for the Playstation 2 console based off of the original LaunchELF. It contains many different features, including a text editor, hard drive manager, as well as network support, and much more.
