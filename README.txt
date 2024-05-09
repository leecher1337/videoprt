Videoport driver patch for VESA in NTVDM
========================================

Originally by Martin Sulak, see:
https://web.archive.org/web/20070817233552/http://www.volny.cz/martin.sulak/

This repository was created to add a branch that works with Windows 7.

Usage
-----
Unzip files, reduce background activity and run "install.bat". Change will apply after restart.

Requirements
------------
VGA with VESA compatible ROM-BIOS
Robust windows driver
Windows 2000 / XP

Compatibility
-------------
OS : tested on W2K SP4, XP SP1
VGA : tested on Voodoo3, S3 Trio3D

Overview
--------
Many NT users complains Ntvdm doesn't "support" VESA. Actually, VESA "support" is not NT's bussiness. Adapter's ROM BIOS is used in fullscreen mode. Many adapters support VESA in BIOS.

But there is another problem with NT : Video driver must inform NT which ports DOS needs. Most if not all drivers are marked as not "VGA comptabile", so vedors can safe coding time byt not supporting fullscreen DOS. In this case "VGA Safe" driver is used instead of hardware specific and this universal driver doesn't know all adapters ports.

Here is solution: Replace port driver with "proxy" dll and hook calls to VideoPortSetTrappedEmulatorPorts function. Usually VGASafe miniport call this function with subset of standard VGA ports. Proxy dll will call real function with modified set of port, which it obtains from registry.
