# PCILeech_DMA_Proxy (WIP)

PCILeech_DMA_Proxy is a DLL that can be injected into other processes to hook common Memory calls such as:
- OpenProcess
- CloseHandle
- ReadProcessMemory
- WriteProcessMemory
- VirtualQueryEx
- VirtualProtectEx
- CreateToolhelp32Snapshot
    - Process32First
    - Process32FirstW
    - Process32Next
    - Process32NextW
    - Module32First
    - Module32FirstW
    - Module32Next
    - Module32NextW
    - Thread32First
    - Thread32Next

More functions are implemented in the **DMALibrary** but are currently not hooked. This is WIP.

These calls are redirected to your target device that your DMA card is connected to.

# Goal

My goal is to write a generic program so almost every program can be proxied to the memory of another device.
I dont know if they work yet, but im planning on doing:
- Some external game cheats/dumpers
- Mimikatz lsass 

# Todo

- [x] Try interactive processes
    - Accepts STDIN
    - It currently does not show the typed text, but its registered

- [x] Better logging

- [] Try actual cheats/dumpers
    - [x] [Aliens: Fireteam Elite Hack](https://github.com/MGreif/aliens-fireteam-elite-external-cheat-menu)
    - [x] [Aliens: Fireteam Elite UnrealEngine Dumper](https://github.com/MGreif/aliens-fireteam-elite-external-cheat-menu)
    - [x] Private CSGO external ESP (Works but i had to manually set the window size as it got the Window automatically)

- [] Hook CreateProcess and other creation api calls to automatically inject proxy DLL into them aswell

# Known caveats
- First few prints (e.g. help/usage) is not printed in child output

# Contents

The Solution contains multiple projects:
- DMALibrary (static lib)
- PCILeech_DMA_Proxy (DLL)
- ProxyLoader (CLI)
- SampleMemoryReader (CLI)

### DMALibrary

This contains the core logic of communicating with the DMA card using @ulfrisk provided API (VMM).

### PCILeech_DMA_Proxy

Contains the hooking logic using [MinHook](https://github.com/TsudaKageyu/minhook) and simply hooks the before mentioned functions.
This DLL can be injected into the target process using any DLL injector. It just needs to be loaded so it can initialize the hooks. 

### ProxyLoader

Sometimes you need to have the DLL injected at the very beginning. You can do this yourself by starting the target process as suspended and injecting te DLL yourself.
But I wanted a [ProxyChains](https://github.com/haad/proxychains) like user experience for it so I decided to write a generic one.
Currently it uses `CreateRemoteThreadEx` to remote execute and `LoadLibraryA` to load the module. But it also contains logic for thread hijacking (did not work on my machine due to Control Flow Guard (CFG)).

#### Usage:

- `ProxyLoader.exe <aboslute-path-to-proxy-dll> <absolute-path-program> [...args]`


### SampleMemoryReader

~~A small DWORD memory reader for testing purposes.~~
A more comprehensive testing tool that tests:
- OpenProcess
- CloseHandle
- CreateToolhelp32Snapshot
    - Process32FirstW
    - Process32NextW
    - Module32FirstW
    - Module32NextW
- VirtualQueryEx
- VirtualProtectEx
- ReadProcessMemory
- WriteProcessMemory

1. It opens the target process by `PID`.
2. It enumerates all processes to get the name of the process with the given `PID`.
3. It enumerates all modules of this process and displays some information about them.
4. It simply reads the supplied vaddr, then writes it, then reads it again.

As I develop the proxy, I will add more functionality to this.

# How to build

0. Check out this repo
1. Open the solution
2. Build DMA Library in Release x64
    - Should create a folder $(SolutionDir)\lib
3. Build PCILeech_DMA_Proxy in Release x64
    - Should create a DLL in the target folder ($(SolutionDir)\x64\Release)

[Optional]
- Build ProxyLoader in Release x64
- Build SampleMemoryReader in Release x64

Run the test:
- Plug in DMA
- Start target process (target pc)
- Use CE or w/e to get your desired vaddr (I used assault cube ammo)
- `ProxyLoader.exe <absolute-path-built-dll> <absolute-path-sample-reader> <pid> <vaddr>`

# Important

The injected DLL will load `VMM.dll`, `leechcore.dll` and `FTD3XX.dll`. Thus you need to place it into a folder inside your PATH env var. E.g. C:\Windows\System32 or next to your target process.
You can get those DLL's in the [MemProcFS](https://github.com/ufrisk/MemProcFS) GitHub repository.


# Disclaimer

This software is designed strictly for educational and forensic research purposes. Its primary intent is to help users understand system behavior, memory analysis, and related security concepts.

I do not condone, support, or encourage cheating, game exploitation, or any violation of a game's Terms of Service (TOS), End User License Agreement (EULA), or other legal agreements. Misusing this tool to gain an unfair advantage in online or offline environments may result in account bans, legal consequences, or other penalties.

By using this software, you acknowledge that you are solely responsible for how you use it. The developer holds no liability for any misuse or consequences that arise from improper application of this tool.

# Credits

Thanks again to @ulfrisk for your amazing work on [PCILeech](https://github.com/ufrisk/pcileech), [MemProcFS](https://github.com/ufrisk/MemProcFS) and [pcileech-fpga](https://github.com/ufrisk/pcileech-fpga). This is such a powerful tool for forensics and offensive (pentest/red-team) duty.

Thanks to Metick, the creator of the [DMALibrary](https://github.com/Metick/DMALibrary) who put so much effort in seamlessly integrating common logic into lowerlevel VMM.dll calls and taking care of many additional functionality (mempages, etc...)
