# wLaunchELF 4.43x_isr_coh

This special version of wLaunchELF has a few extra features

- Execution of ELF files without the need for .ELF extension (eg: `mc0:*LOAD` files used by most arcade games)
- Execution of IRX files with or without the .IRX extension

## Important notes

### System 2x6 Video display init

if your wLaunchELF for arcade will be installed as `boot.bin` on a system2x6 you will need a method to initialize the JVS Video display (otherwise, nothing will come out of your VGA Outputs). here you have a [standalone IRX module](https://github.com/user-attachments/files/19588377/SYSTEM2x6_VIDEO_INIT.zip) that can do that

wLaunchELF will look for it in these path, following this order:

- `./ACJVLOAD.IRX`
- `mc0:/ACJVLOAD.IRX`
- `mmce0:/COH/ACJVLOAD.IRX`
- `mc1:/ACJVLOAD.IRX`
- `mass0:/ACJVLOAD.IRX`
- `mass1:/ACJVLOAD.IRX`

if the module is successfully executed, when the UI pops in you will see `JVS INIT` on top

### Arcade WatchDog

Arcade PS2s will reset themselves if they spend between 3 and 5 minuts (depending on model) without authenticating the security dongle

By default, wLaunchELF will not deal with this. as it involves making the access to `mc0:` rather unstable

To keep the watchdog happy, please create a file called `watchdog.opt`, wich must be located on one of these locations:

- `mc0:/watchdog.opt`
- `mmce0:/watchdog.opt`
- `mc1:/watchdog.opt`
- `mass:/watchdog.opt`
- `host:/watchdog.opt`

what this will do is just execute the `rom0:DAEMON` module. this module setups a thread that calls the card authentification routine on the mc0 directly, every minute. keeping the watchdog happy

if the module is successfully executed, when the UI pops in you will see `watch OK` on top (else `watch NG`)

### IOP Boot List

If your setup needs wLaunchELF to load a specific set of IRX modules, create a file called `IOPBOOT.CNF` wich has to be placed on the root of any of the memory cards

the format is the following:

```ini
# example of custom IOP Boot list. these modules are executed in order before program UI is ready
# NAME = PATH
ACUART = mc0:acuart.irx
ACTTY = mc0:actty.irx
LED = rom:LED
WHATEVER = mc1:FOO.IRX
```

the name is used to print on screen in case some error POPS in when launching the associated module
