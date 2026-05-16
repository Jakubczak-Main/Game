# Game
Making a game.

## Build

```powershell
cmake -S . -B build-mingw -G "MinGW Makefiles"
cmake --build build-mingw
.\build-mingw\Game.exe
```

The first configure downloads GLFW and GLAD through CMake FetchContent.
GLAD generates the OpenGL loader locally, so CMake will tell you if the Python
package `Jinja2` is missing and show the exact `pip` command to install it.

If MinGW Makefiles prints an error like `'...\Programs' is not recognized`, the
compiler is probably installed in a path that `cmd.exe` mis-parses. On this PC,
this configure command uses Windows short paths for the compiler:

```powershell
cmake -S . -B build-mingw-short -G "MinGW Makefiles" `
  -DCMAKE_C_COMPILER=C:/Users/matys/Desktop/PROGRA~1/PROGRA~1/Compiler/bin/gcc.exe `
  -DCMAKE_CXX_COMPILER=C:/Users/matys/Desktop/PROGRA~1/PROGRA~1/Compiler/bin/C__~1.EXE
cmake --build build-mingw-short
```
