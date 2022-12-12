# Solaris 11 x86 ATA Driver Backport to Solaris 10 SPARC

Silly little project so I could use a Silicon Image 3112 SATA Controller card on my Sun Blade 150, as it has OpenFirmware support but no Solaris 10 SPARC driver. As the Solaris SPARC ATA driver is closed source, and most of the functions necessary for registering an ATA card are declared static, I decided it'd be easier to simply backport the Solaris 11 x86 driver to Solaris 10 SPARC.

|Module|Dependencies|
| ---- | ---------- |
|drv/sol11ata||
|drv/sol11cmdk|misc/sol11dadk, misc/sol11strategy, misc/cmlb|
|misc/sol11dadk|misc/sol11gda|
|misc/sol11gda||
|misc/sol11strategy||