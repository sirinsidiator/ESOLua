This is ESOLua a fork of Lua 5.1 with modifications for Elder Scrolls Online.
The goal of this project is to provide a runtime that can be used to test AddOns for the game outside the game client.
It won't be possible to provide all the functionality of the game client, but over time ESOLua should gain a reasonable amount of features.

The game API functions are all provided by the `eso` module. In order to use them you will have to manually assign them to global variables as needed.
AddOns can be loaded with the `eso.LoadAddon` function. Simply pass the relative path to a manifest file to it.

In order to build the executable you will need to install MinGW and call `build.bat` in the project root.
Afterwards you can try it by running the batch files from within the `examples` folder.