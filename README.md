# Swiss cMIOS

This is a variant of the debug cMIOS that bundles Swiss r1788 into the cMIOS, and otherwise works identically to WiiGator cMIOS. See the main branch readme for more info on the debug cMIOS project and original cMIOS installer.

**Technical decisions**  
- `resetDI` is called for injected Swiss because not doing so causes an extra drive reset in current versions of Swiss (r1761+).
- `resetDI` is not called in legacy cMIOS mode because doing so softlocks older versions of Swiss (–r1760) that don't do any Wii DI resetting.
- `consoleInit` is required for gamepad scanning to work; I don't know the exact steps required, but `videoInit` (from monke/console) and `PAD_Init` are together insufficient.

## Old cMIOS Installer Readme

Installer for WiiGator's cMIOS (with MIOSv10 as base)

What is this?
This program patches the MIOS to allow booting of gamecube homebrew on a Wii and also patches the MIOS to enable DVD-R access. For gamecube homebrew and backups you need a loader that boots gamecube homebrew/boots gamecube games with a gamecube homebrew loader, for example NeoGamma R9.

Hint:  
Get the required MIOS files as wad on sd card before starting HMP. You can easiliy download them with NUS Downloader
(see WiiBrew.org for download link). The expected name is:
RVL-mios-v10.wad


This application is based on PatchMii by bushing, svpe and tona  
and also contains code from:  
Raven's Menu Loader Clone (IOS selection code)  
Waninkoko's WAD Manager (WAD code)  
AnyTitleDeleter (TITLE_UPPER and TITLE_LOWER)  
WiiGator's cMIOS installer(DVD-R and homebrew patch)  

(please notify me if you find other code from other people inside this)

WiiPower
