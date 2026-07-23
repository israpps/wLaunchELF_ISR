# __wLaunchELF_R3Z__

wLaunchELF, formerly known as uLaunchELF, also known as wLE or uLE (abbreviated), is an open source file manager and executable launcher for the Playstation 2 console based off of the original LaunchELF. It contains many different features, including a text editor, hard drive manager, as well as network support, and much more.

## Supported devices:
| Device | Description | Visibility |
|---|---|---|
| __mc:/__ | Memory Cards | Always |
| __usb:/__ | Fat/exFAT (BDM) USB | Always |
| __mmce:/__[^1] | Multi Purpose Memory Card Emulator IE SD2PSX, PSxMemCard Gen2 or MemCard Pro 2. | Always |
| __mx4sio:/__[^1] | SD card interface over memory card port. | Always |
| __hdd:/__[^2] | APA formatted internal HDD | Hidden on deckard |
| __ata:/__[^2] | BDM hard drive, exFAT for now till more are supported | Hiddon on deckard |
| __xfrom:/__ | PSX DESR-XXXX flash storage | Hidden on non-PSX |
| __dvr_hdd0:/__ | PSX DESR-XXXX digital video recorder hdd partition/side | Hidden on non-PSX |
| __cdfs:/__ | CD/DVD File System | Always |
| __udpfs:/__ | Network interface used with [PCM720s UDPFSD Server](https://github.com/pcm720/udpfsd) | Always |

Drivers load on demand for maximum compatibility and initial boot speed.

[^1]: MMCE and MX4SIO will incure an IOP reboot as the 2 are incompatible.

[^2]: Dual HDD/ATA support is built in for future development.

## Features:
- RetroGem Game ID for [PIXEL FX RetroGem](https://www.pixelfx.co/hdmi-retro-gem)
- Launch PS1 VCDs with option for custom POPStarter path, falls back to defaults.
- Writes to history file for disc launches for mmce vmc change
- Applies deckard disc patches
- X/0 applied per region if no config file is found so O is only default for Japan
- Keyboard layout choices: QWERTY, DVORAK, AZERTY, QWERTZ, ABNT, ABC
- Language built: English, Spanish, Italian, French, German, Polish, Portuguese, Portuguese Brazilian, Hungarian
- All config options exposed in gui
- HDD/ATA drives hidden for deckard ps2 (SCPH-75K+)
- Xfrom/dvr_hdd0 hidden from non-PSX consoles
- Dual hdd support
- multi-usb support
- HDD Manager can inject APA partition headers from header files on `usb:`, `mmce0:`, `mmce1:`, or `udpfs:` for HDD-OSD icons/launch metadata
- All drivers besides core lazy loading for faster boot and support of all devices.
- Create and Extract PSU options so user knows what will happen
- Extra file extensions for Text Editor ShortCuts
- Timestamp manipulation feature to fix the date of any memory card folder containing any icon-based exploit _(\*tuna)_
- Launch KELFs (encrypted elfs)
- [LaunchELF with Args](#launchelf-with-args)
- [APA Header injection](#HDD-APA-header-injection)
- Support for PS3/PS4 Dualshocks thanks to Alex Parrado (DS34 build)

### LaunchELF with Args

<details>

<summary>LaunchELF with Args</summary>

In FileBrowser, select a TextEditor-supported file, press `R1`, then choose `LaunchELF with Args`.
wLaunchELF parses the file immediately, caches the args in memory, and leaves you in FileBrowser to select the target `ELF`.

The same action is available from TextEditor's `R1` menu for the active opened file.
From TextEditor the current in-memory buffer is parsed, so unsaved edits are used.
After caching args, wLaunchELF returns to FileBrowser so you can choose the target executable.

For normal executable launches, wLaunchELF also checks for a sidecar arg file named `<elf name>.arg` in the same folder, for example `BOOT.arg` for `BOOT.ELF`.
Sidecar args use the same parser and limits.
If args were already cached through `R1` -> `LaunchELF with Args`, the sidecar `.arg` file is ignored.

Each non-empty line becomes one argument.
Line endings are stripped; other characters on the line are preserved.

| Limit | Value |
|---|---:|
| User arguments | 12 lines |
| Characters per argument | 255 |
| Total cached argument text | 2,048 bytes including terminators |

The target receives the normal selected/handoff path as `argv[0]`, followed by the cached line arguments.

</details>

### HDD APA header injection

<details>

<summary>HDD APA header injection</summary>

The HDD Manager R1 menu includes `Inject Header` for PFS partitions.
Source devices are `usb:`, `mmce0:`, `mmce1:`, and `udpfs:`.
After choosing a source device, choose one injection mode:

| Mode | Behavior |
|---|---|
| `This partition only` | Injects only the currently selected PFS partition from `device:/__Headers/<selected partition>/`. |
| `Matching partitions` | Scans `device:/__Headers/` and injects every existing PFS partition with a matching folder name. |
| `Matching and create missing partitions` | Injects existing matches, then prompts for each valid header folder that has no matching partition. `No` skips that folder and continues; cancel stops the bulk operation. |

Header files must be placed in a flat folder matching the partition name:

`device:/__Headers/<matching partition name>/`


| File | Requirement | Destination | Maximum File Size | Format / Purpose | Notes |
|---|---|---|---:|---|---|
| `system.cnf` | Required | APA header | 512 bytes | Partition configuration | See [example](#example-systemcnf). |
| `icon.sys` | Required on PS2 | APA header | 1,024 bytes | Icon metadata | Not listed as required for PSX/DVR environments. |
| `list.ico` | Required on PS2 | APA header | 1,112,064 bytes | HDD-OSD icon | [Icon Generator](https://github.com/CosmicScale/HDD-OSD-Icon-Generator)  |
| `boot.kelf` or `BOOT.KELF` | Optional | APA header | 978,944 bytes | Executable KELF | Use KELFTool. |
| `BOOT.ELF` | Optional | `pfs:/BOOT.ELF` | N/A | Partition executable | Useful when the injected `boot.kelf` or `BOOT.KELF` is a KELF forwarder such as [OSDMenu Launcher](https://github.com/pcm720/OSDMenu/tree/main/launcher) |
| `info.sys` | Optional | `pfs:/res/info.sys` | N/A | Partition information used by PSBBN or PSX XMB | See [example](#example-infosys). |
| `jkt_001.png` | Optional | `pfs0:/res/jkt_001.png` | N/A | PSBBN image | 256x256 PNG 8-bit indexed color 32-bit RGBA palette. |
| `jkt_002.png` | Optional | `pfs0:/res/jkt_002.png` | N/A | PSX XMB image | 76x108 PNG 8-bit indexed color 32-bit RGBA palette. |
| `jkt_cp.png` | Optional | `pfs0:/res/jkt_cp.png` | N/A | PSX XMB copyright image | 290 pixels wide, 46-300 pixels high, 32-bit RGBA PNG. |

#### Example system.cnf

```ini
BOOT2 = pfs:/boot.kelf
VER = 1.00
VMODE = NTSC
HDDUNITPOWER = NICHDD
```

#### Example info.sys

```ini
title = [SYS] R3CONFIGURATOR
title_id = SYS-R3CONFI
title_sub_id = 0
release_date =
developer_id =
publisher_id = pcm720, R3Z3N
note =
content_web =
image_topviewflag = 0
image_type = 0
image_count = 1
image_viewsec = 600
copyright_viewflag = 0
copyright_imgcount = 0
genre =
parental_lock = 1
effective_date = 0
expire_date = 0
violence_flag = 0
content_type = 255
content_subtype = 0
```

</details>

### Build shortcuts

- `make all-ds34-off` builds a dedicated ELF without DS34 support.
- `make all-ds34-on` builds a dedicated ELF with DS34 support.
- `make all-ds34-variants` builds both variants in one run.

