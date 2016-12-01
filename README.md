# The Hacker's Sandbox

Thank you for your interest in this software! The Hacker's Sandbox (THS) is a
simulation engine of hacking on Unix-like systems. It allows you to play various
mission packs created by the engine's author or the community.

The default mission pack is a small game called "Destiny". In this game, don the
role of a conscripted computer hacker, fighting your way through several
machines to gain both root... and your freedom...


## Playing the game

In order to play the default mission pack, all you have to do is run the sandbox
binary for your system. On OSX and Linux, this is "sandbox". On Windows OSes
this binary is called "sandbox.exe"

On many systems, it is often easier to open a command line and run the binary
from there, rather than double-clicking from the GUI. (This is especially true
for OSX).

For example, if you downloaded the game to your Downloads folder on a Mac, you
can open the Terminal app and type the following:

~~~
$ unzip ~/Downloads/ths.osx.zip
$ cd ~/Downloads/ths.osx/
$ ./sandbox
~~~

To play other mission packs, simply append the mission pack name to the end of
your command.

For version information and a list of changes, please read the CHANGELOG file
that accompanies this software.


## Viewing the source

The Hacker's Sandbox is now open source! You are free to peruse and modify the
code. This isn't quite as elegant as it should be, and there are too many reasons
for that to list here. A lot of it revolves around old discussions and legacy
designs. Suffice to say, it should all work.

It's worth pointing out, the original save file format is a bit odd. It uses a
*slightly* modified version of base64 (named ab64) to pack data in newline
separated rows. This was an intentional design decision stemming from some long
conversations with a few friends. The intention was to be a sort of easy puzzle
for novice reverse-engineers to solve. Now that this is open source it doesn't
make sense anymore, so it has been replaced with a JSON-based format. However,
the legacy code is kept for backwards compatibility with old saved-game files.

