# Dolphin - A GameCube / Wii / Triforce Emulator

[Homepage](https://dolphin-emu.org/) | [Project Site](https://github.com/dolphin-emu/dolphin) | [Forums](https://forums.dolphin-emu.org/) | [Wiki](https://wiki.dolphin-emu.org/) | [Issue Tracker](https://code.google.com/p/dolphin-emu/issues/list) | [Coding Style](https://github.com/dolphin-emu/dolphin/blob/master/Contributing.md) | [Transifex Page](https://www.transifex.com/projects/p/dolphin-emu/)


## An iOS version
I am 100% sure this doesn't work at the moment. It simply crashes when you try to run the emulator. The code formatting and style guidelines have probably gone out the window for now, and there's also a bunch of random and unused Xcode stuff lying around.

If you still want to try to build, here's what you'll need:

<ul>
<li>A Mac with Xcode installed (yes, it can be a Hackintosh)</li>
<li>A Jailbroken iOS device runnning iOS 8.0 and above</li>
<li>(Optional) A Code Signing Identity from the Apple Developer Program</li>
</ul>

Once you have everything, you can now build by doing the following:
<ul>
<li>Install brew.</li>
<li>Install cmake with brew.</li>
<li>Run the following in the directory where you cloned this repository:</li>
</ul>
```
mkdir Build
cd Build
cmake -DCMAKE_TOOLCHAIN_FILE=../Source/iOS/ios.toolchain.cmake -DCMAKE_BUILD_TYPE=Release -DDISABLE_WX=true -DENABLE_PCH=false ..
make
```
<ul>
<li>Copy all .a files in Build's subdirectories in Source/iOS/DolphiniOS/libraries</li>
<li>Copy all .h files in Source's subdirectories in Source/iOS/DolphiniOS/libraries/include</li>
</ul>

### To install the application: 

<ul>
<li>If you don't have them already, install the "AppSync Unified" (on Karen/angelxwind's repo), "sudo", and "OpenSSH" packages in Cydia.</li>
<li>If you don't have a signing identity, make sure that you've modified SDKSettings.plist (see Google for more information) and set Code Signing Settings to 'Ad Hoc Code Sign'. If you do, make sure that you set your Code Signing Settings to 'iOS Developer'.</li>
<li>Plug in your 64-bit iOS device and make sure the destination (upper left) is set to that device.</li>
<li>Click the run (play) button in the upper left to install the application.</li>
<li>Exit the application, then kill it in the App Switcher (Multitasking).</li>
<li>SSH into your device and run: (replace "(UUID)" with the long string of characters, numbers, and dashes from the result of the first command)</li>
</ul>
```
find ~/Containers/Bundle/Application -name 'DolphiniOS'
sudo cp -R ~/Containers/Bundle/Application/(UUID)/DolphiniOS.app /Applications/
```
<ul>
<li>Delete the application from your home screen.</li>
<li>Run:</li>
</ul>
```
uicache
```
<ul>
<li>Run the application.</li>
</ul>

### To replace the application with a new copy:

<ul>
<li>Kill any existing instances of the application in the App Switcher.</li>
<li>Run:</li>
</ul>
```
sudo rm -rf /Applications/DolphiniOS.app
uicache
```

You can now install a new copy of the app via Xcode.

## Original Dolphin Readme

Dolphin is an emulator for running GameCube, Wii, and Triforce games on
Windows, Linux, OS X, and recent Android devices. It's licensed under
the terms of the GNU General Public License, version 2 or later (GPLv2+).

Please read the [FAQ](https://dolphin-emu.org/docs/faq/) before using Dolphin.

## System Requirements
* OS
    * Microsoft Windows (Vista or higher).
    * Linux.
    * Apple Mac OS X (10.9 or higher).
    * Unix-like systems other than Linux might work but are not officially supported.
* Processor
    * A CPU with SSE2 support.
    * A modern CPU (3 GHz and Dual Core, not older than 2008) is highly recommended.
* Graphics
    * A reasonably modern graphics card (Direct3D 10.0 / OpenGL 3.0).
    * A graphics card that supports Direct3D 11 / OpenGL 4.4 is recommended.

## Installation on Windows
Use the solution file `Source/dolphin-emu.sln` to build Dolphin on Windows.
Visual Studio 2013 is a hard requirement since previous versions don't support
many C++ features that we use. Other compilers might be able to build Dolphin
on Windows but have not been tested and are not recommended to be used.

An installer can be created by using the `Installer_win32.nsi` and
`Installer_x64.nsi` scripts in the Installer directory. This will require the
Nullsoft Scriptable Install System (NSIS) to be installed. Creating an
installer is not necessary to run Dolphin since the Build directory contains
a working Dolphin distribution.

## Installation on Linux and OS X
Dolphin requires [CMake](http://www.cmake.org/) for systems other than Windows. Many libraries are
bundled with Dolphin and used if they're not installed on your system. CMake
will inform you if a bundled library is used or if you need to install any
missing packages yourself.

### Build Steps:
1. `mkdir Build`
2. `cd Build`
3. `cmake ..`
4. `make`

On OS X, an application bundle will be created in `./Binaries`.

On Linux, it's strongly recommended to perform a global installation via `sudo make install`.

## Installation on Android
Dolphin requires [Android Studio](http://developer.android.com/tools/studio/index.html) to build
the Android UI. Import the Gradle project located in `./Source/Android`, and then execute the
Gradle task `assembleDebug` to build, or `installDebug` to install the UI onto a connected device.

In order to launch the app, you must build and include the native Dolphin libraries into the UI project.
(Building native code requires the [Android NDK](https://developer.android.com/tools/sdk/ndk/index.html).)
Android Studio will do this for you if you create `Source/Android/build.properties`, and place the
following inside:

```
makeArgs=<make-args>
```

Replace `<make-args>` with any arguments you want to pass to `make`. If you need to use a specific
version of git, cmake, or the NDK, you can also add `gitPath=<path>`, `cmakePath=<path>` or
`ndkPath=<path>`, replacing `<path>` with the actual paths. Otherwise, these will be found
automatically. Then execute the `assembleDebug` or `installDebug` task corresponding to the
hardware platform you are targeting. For example, to deploy to a Nexus 9, which runs the AArch64
architecture, execute `installArm_64Debug`. A list of available tasks can be found in Android
Studio in the Gradle tray, located at the top-right corner of the IDE by default.

The native libraries will be compiled, and copied into `./Source/Android/app/libs`. Android Studio
and Gradle will include any libraries in that folder into the APK at build time.

## Uninstalling
When Dolphin has been installed with the NSIS installer, you can uninstall
Dolphin like any other Windows application.

Linux users can run `cat install_manifest.txt | xargs -d '\n' rm` as root from the build directory
to uninstall Dolphin from their system.

OS X users can simply delete Dolphin.app to uninstall it.

Additionally, you'll want to remove the global user directory (see below to
see where it's stored) if you don't plan to reinstall Dolphin.

## Command Line Usage
`Usage: Dolphin [-h] [-d] [-l] [-e <str>] [-b] [-V <str>] [-A <str>]`  

* -h, --help Show this help message  
* -d, --debugger Opens the debugger  
* -l, --logger Opens the logger  
* -e, --exec=<str> Loads the specified file (DOL,ELF,WAD,GCM,ISO)  
* -b, --batch Exit Dolphin with emulator  
* -V, --video_backend=<str> Specify a video backend  
* -A, --audio_emulation=<str> Low level (LLE) or high level (HLE) audio  

Available DSP emulation engines are HLE (High Level Emulation) and
LLE (Low Level Emulation). HLE is fast but often less accurate while LLE is
slow but close to perfect. Note that LLE has two submodes (Interpreter and
Recompiler), which cannot be selected from the command line.

Available video backends are "D3D" (only available on Windows) and
"OGL". There's also "Software Renderer", which uses the CPU for rendering and
is intended for debugging purposes only.

## Sys Files
* `totaldb.dsy`: Database of symbols (for devs only)
* `GC/font_ansi.bin`: font dumps
* `GC/font_sjis.bin`: font dumps
* `GC/dsp_coef.bin`: DSP dumps
* `GC/dsp_rom.bin`: DSP dumps
* `Wii/clientca.pem`: Wii network certificate
* `Wii/clientcacakey.pem`: Wii network certificate
* `Wii/rootca.pem`: Wii network certificate

The DSP dumps included with Dolphin have been written from scratch and do not
contain any copyrighted material. They should work for most purposes, however
some games implement copy protection by checksumming the dumps. You will need
to dump the DSP files from a console and replace the default dumps if you want
to fix those issues.

Wii network certificates must be extracted from a Wii IOS. A guide for that can be found [here](https://wiki.dolphin-emu.org/index.php?title=Wii_Network_Guide).

## Folder Structure
These folders are installed read-only and should not be changed:

* `GameSettings`: per-game default settings database
* `GC`: DSP and font dumps
* `Maps`: symbol tables (dev only)
* `Shaders`: post-processing shaders
* `Themes`: icon themes for GUI
* `Resources`: icons that are theme-agnostic
* `Wii`: default Wii NAND contents

## User Folder Structure
A number of user writeable directories are created for caching purposes or for
allowing the user to edit their contents. On OS X and Linux these folders are
stored in `~/Library/Application Support/Dolphin/` and `~/.dolphin-emu`
respectively. On Windows the user directory is stored in the `My Documents`
folder by default, but there are various way to override this behavior:

* Creating a file called `portable.txt` next to the Dolphin executable will
  store the user directory in a local directory called "User" next to the
  Dolphin executable.
* If the registry string value `LocalUserConfig` exists in
  `HKEY_CURRENT_USER/Software/Dolphin Emulator` and has the value **1**,
  Dolphin will always start in portable mode.
* If the registry string value `UserConfigPath` exists in
  `HKEY_CURRENT_USER/Software/Dolphin Emulator`, the user folders will be
  stored in the directory given by that string. The other two methods will be
  prioritized over this setting.


List of user folders:

* `Cache`: used to cache the ISO list
* `Config`: configuration files
* `Dump`: anything dumped from Dolphin
* `GameConfig`: additional settings to be applied per-game
* `GC`: memory cards and system BIOS
* `Load`: custom textures
* `Logs`: logs, if enabled
* `ScreenShots`: screenshots taken via Dolphin
* `StateSaves`: save states
* `Wii`: Wii NAND contents

## Custom Textures
Custom textures have to be placed in the user directory under
`Load/Textures/[GameID]/`. You can find the Game ID by right-clicking a game
in the ISO list and selecting "ISO Properties".
