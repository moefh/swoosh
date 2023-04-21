# swoosh

This is a small portable GUI program (using
[wxWidgets](https://www.wxwidgets.org/)) that sends and receives
messages between computers within a local network. It doesn't use a
server and you don't need to mess with IP addresses -- just start it
and you're good to go. It has been tested on Ubuntu (x86_84), Debian
(ARM/Raspberry Pi) and Windows 10.

![Swoosh window on Linux](doc/swoosh-window.png)

When you send a message, `swoosh` broadcasts a small UDP packet (the
beacon) announcing the new message for everyone in your local network.
Other instances of `swoosh` in other computers that receive the beacon
then connect to the sender to retrieve the message contents.

By default, `swoosh` listens and broadcasts on UDP port 5559. Messages
are transmitted on TCP ports 5559-5659 (a free port in this range is
chosen every time).

# Compilation

## Linux

On Linux, just make sure you have the development packages for
wxWidgets installed (`libwxgtk3.0-gtk3-dev` on Ubuntu and Debian) and
run `make`.

## Windows

On Windows, I used Visual Studio Community 2022 (although it's also
probably possible to use mingw). To compile using Visual Studio:

- download the windows binaries from the
[wxWidgets site](https://www.wxwidgets.org/downloads/) (headers,
development files and release DLLs)

- extract the packages on some directory (here I'll call it
`c:\wxwidgets`)

- rename the directory `c:\wxwidgets\lib\vc14x_x64_dll` to
`c:\wxwidgets\lib\vc143_x64_dll` (here "`143`" should be the Windows
SDK version used by your Visual Studio, VS 2022 uses SDK 143)

- add an environment variable on Windows named `wxwin` containing
`c:\wxwidgets`

- add `c:\wxwidgets\lib\vc143_x64_dll` to your PATH environment variable

- open the `swoosh` solution (`swoosh.sln`) and build it.
