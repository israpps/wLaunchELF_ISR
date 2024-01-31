# __wLaunchELF 4.43x_isr__
[![Codacy Badge](https://api.codacy.com/project/badge/Grade/5e07db76c668493d888a7f9b97d79821)](https://app.codacy.com/gh/israpps/wLaunchELF_ISR?utm_source=github.com&utm_medium=referral&utm_content=israpps/wLaunchELF_ISR&utm_campaign=Badge_Grade_Settings)
[![Automated-Build](https://github.com/israpps/wLaunchELF_ISR/actions/workflows/compile.yml/badge.svg)](https://github.com/israpps/wLaunchELF_ISR/actions/workflows/compile.yml)

[![Static Badge](https://img.shields.io/github/downloads/israpps/wLaunchELF_ISR/total?style=for-the-badge&logo=protondrive&logoColor=00CCFF&label=DOWNLOAD&labelColor=000000)](https://israpps.github.io/20210113_wLaunchELF_isr/#download)

_this uLaunchELF mod was my first PS2 project._

It features:

- Timestamp manipulation feature to fix the date of any memory card folder containing any icon-based exploit _(\*tuna)_
- Extra file extensions for Text Editor ShortCuts
- ~`100kb` smaller that it´s original counterpart (wLE 41e4ebe) (this was possible thanks to CI with ps2dev:v1.0 toolchain)
- Support for PS3/PS4 Dualshocks thanks to Alex Parrado
#### this mod has proven to be excellent for HDD, USB and MC management.

> this mod is already bundled on any mod/project/repack made by me (and it´s auto-updated if that project is hosted here on github)

### Explanation of download filenames

> release filename change according to the enabled features:

- `BOOT`: Base filename, means nothing
- `UNC`: Executable is Uncompressed
- `SIO_DEBUG`: Supports printing debug information via the EmotionEngine serial port (useless unless your PS2 has serial port attached)
- `NO_NETWORK`: Network features are disabled and network IRX drivers stripped away, with the purpose of making a smaller wLaunchELF for users who don't use network
- `XFROM`: Support for accessing the [PSX-DESR](https://upload.wikimedia.org/wikipedia/commons/f/fa/Console_psx.jpg) internal flash memory
- `EXFAT`: Support for accessing EXFAT filesystems from BDM devices (USB & MX4SIO)
- `DS34`: Support for use of PlayStation 3 and PlayStation 4 controllers
- `MX4SIO`: Support for browsing the contents of SD Cards connected via mx4sio

> to see the enabled features on an already downloaded version of this mod, please go to `Filebrowser > MISC/ > BuildInfo`

# **original readme**
wLaunchELF, formerly known as uLaunchELF, also known as wLE or uLE (abbreviated), is an open source file manager and executable launcher for the Playstation 2 console based off of the original LaunchELF. It contains many different features, including a text editor, hard drive manager, as well as network support, and much more.
