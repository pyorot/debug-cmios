# Debug cMIOS

This cMIOS is an evolution of the WiiGator/WiiPower cMIOS, forked from its repository (backed up with user-downloadable release [here](https://github.com/ForwarderFactory/cmios-wiigator-installer/)). It's an unreleased research project, with a write-up of its current state below.

## The new cMIOS

The old cMIOS, when used with [Wii Swiss Booter](https://github.com/pyorot/wii-swiss-booter/), often fails to load Swiss. The issue was first investigated [here](https://github.com/pyorot/wii-swiss-booter/issues/4). The debug cMIOS in this repository added a console to print debug output, and then investigations began.

### Background: MIOS
MIOS ("Mini IOP Operating System") is installed as a system title on the Wii, and comprises both an ARM binary (which runs on IOP (= Starlet)) and a PPC binary (which runs on Broadway). The old cMIOS was constructed by patching a couple of minor bits of MIOS code (to enable DVD-R and Action Replay), and injecting a new program into the PPC binary, with a hook to redirect the PPC entrypoint to it, whereupon it either tries to boot Homebrew or calls the original entrypoint for normal MIOS behaviour. This injected program will be called "cMIOS" for simplicity ("c" for "custom").

### Background: GameCube mode
The boot process for GameCube mode functions as follows:
1. Something running on Broadway launches the BC system title.
2. BC (a variant of boot1) runs on IOP, halts Broadway, and changes the hardware (e.g. clock speed) to GameCube mode.
3. BC loads boot2, which loads the ARM MIOS executable.
4. ARM MIOS loads data to Napa (= MEM1, a RAM chip), e.g. app-loaders and the PPC MIOS entrypoint, and resumes Broadway.
5. Broadway runs the PPC MIOS (or cMIOS).

### Investigation
Wii Swiss Booter communicates with the old cMIOS via shared memory. But the memory isn't owned by anything, as a result of the Booter exiting, CPU halting, chain-load into the cMIOS and CPU resuming described above. So, this is undefined behaviour. Indeed, to use the old cMIOS, a frontend app like the Booter writes "gchomebrew dol" to 0x807fffe0 and the .dol payload to 0x80800000 (the .dol has a header so the cMIOS can infer the payload's size from just its location).

The first thing I added was hash verification of the payload. I have an old Wii (motherboard 01) and a new Wii (motherboard 60), and as said in the original issue, the new Wii booted Swiss completely reliably, and the old Wii never booted it with a compressed .dol and went ~70% with an uncompressed one. 

The new Wii always passes this verification and the old Wii almost never does (maybe 5% success rate). This is regardless of where in MEM1 the payload is loaded.

I cleaned up the cMIOS to use a signalling struct written to and read from 0x80001800, which is an area of memory that is supposed to never be cleared and is usually used by either the Gecko cheat interpreter or by Wii homebrew as a stub for reloading to the Homebrew Channel. Since reloading to Wii mode from GameCube mode requires restarting the Wii, I feel using this memory area for chain-loading in GameCube mode is a good design move for the new cMIOS. The signalling struct has an array of pointers specifying where the payload is written to, and the debug cMIOS reads it from there.

I say "array" – the next thing I added was redundancy. The booter makes 5 copies of the payload .dol in MEM1, and the cMIOS has an error-correcting ("ecc") mode where it reconstructs the original payload via this basic algorithm: going word-by-word (a word is a 4-byte memory value), where [n] (n = 0, ... , 4) is that word in payload n:
```
     if [0] != [2] and [2] == [4]: [0] = [2]
else if [0] != [1] and [1] == [3]: [0] = [1]
else if [0] != [1] and [1] == [2]: [0] = [1]
```

(I feel like higher memory locations are less reliable than lower). This isn't a real ECC algorithm, but it could be improved.

This seemed to work alright: here's the [test](https://youtu.be/QunFf7MBbR0). 80% success rate is a huge improvement over 5% on the old Wii, and the new Wii will work unchanged. The memory seems to warm up, so earlier runs are less likely to succeed, so it's still a poor solution for users.

### Diagnosis

This is kind of like reverse [Stop 'n' Swop](https://banjokazooie.fandom.com/wiki/Stop_%27n%27_Swop). The old cMIOS exploited stale memory that, it seems, was meant to not be cleared but was inadvertently being corrupted on older hardware revisions of the Wii.

Ultimately, the intended route for loading GameCube software is to use an app-loader that reads it into RAM from a different medium – intended to be a disc, but could also be from the MIOS binary or any device GameCube mode has access to.

### Future Design

As a first step, I linked a swiss.dol payload into the cMIOS (via DevkitPPC's standard binary data linking), and added a new codeword into the signalling struct to tell the cMIOS to read it from there. That commit required [hardly any new code](https://github.com/pyorot/debug-cmios/commit/39fa86c5a76f8eda8c9609874d2793d62e5d7921) and just worked as expected. You can try it out if you bring your own swiss.dol file. So, we have a 100% reliability solution for booting Swiss on older Wiis now 👏.

This isn't ready for release for two reasons.
1. We need to compile a new cMIOS for a given payload.
2. Changes in the cMIOS are a nuisance for [system title integrity checking](https://github.com/systemwii/libsystitver/). In other words, we can't distribute a "standard" cMIOS anymore.

As such, I think we want to reform the cMIOS installer into an app that injects .dol files from an SD card or USB drive into the cMIOS (then patches it into an official MIOS and installs the result as before). It would be extra nice if this could be installed to the Wii's NAND as several content files, so that the first one would be the entrypoint/switcher stub and would always be verifiably the same.

In order to do this, we must do dynamic payload injection. The cMIOS installer already does this by adding it to the PPC binary's .elf in memory as a new *segment*. Swiss has its own setup where it adds it to the Wii Booter's .dol as a new *section*. Idk what the best option is but it's all a lot of faffing around I haven't done yet, since usually this kind of thing is done statically, and most Q&As online talk about that.

**UX**  
The switcher stub could be designed to accept button inputs [similarly to gekkoboot](https://github.com/redolution/gekkoboot#usage). With a 3MiB limit, it could load 4 payloads to `0x80400000`, `0x80700000`, `0x80a00000` and `0x80d00000`, and let us choose which to execute by holding A/B/X/Y. We could also include the stale-memory methods, loading from file with or without error-correction, as long as we verify hashes in those cases.

The stale-memory methods require the signalling struct, but the button-holding doesn't, and so we could add a hook to Priiloader to just launch installed GameCube homebrew by doing `WII_LaunchTitle(BC)` and not much else, which means Swiss and other GameCube apps would just be installed, auto-bootable, nice. All the user has to do is run the installer and give it some .dol files.

It is a shame that GameCube homebrew must be installed to work reliably on all Wiis, kinda harking back to the GameCube Backup Launcher and DIOS MIOS days, but it's what it's.

### Regressions

Currently, the debug cMIOS crashes 5% of the time in both file loading and injected mode, soon after LaunchTitle is called in the Booter. There's a missing line from Wii Swiss Booter's DolLoader that sets a clock that might prevent this, or else the [Wii app code](https://github.com/emukidid/swiss-gc/blob/master/wii/booter/source/main.c) from Swiss's own Wii Booter can be implemented here.

Note also that the installer's codebase is a mess (I only cleaned up the cMIOS itself). I've patched it to auto-install the cMIOS using an `RVL-MIOS-v10.wad` in the root of an inserted USB drive, but turning it into a full release would require fixing up some very, very concerning compiler warnings.

```
src/IOSPatcher.c:146:9: warning: 'memset' specified bound 4294967292 exceeds maximum object size 2147483647 [-Wstringop-overflow=]
  146 |         memset(sig_ptr + 4, 0, SIGNATURE_SIZE(sig)-4);
```

And is just generally a lot of work because of the Wii homebrew community's inability to provide things like a standard WAD library. For debug purposes, the hacked installer here works Fine™. And it doesn't require a cIOS lmao.

## Readme for the old cMIOS

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
