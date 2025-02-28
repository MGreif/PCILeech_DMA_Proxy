# PCILeech_DMA_Proxy
A small DLL that once injected, hooks common windows memory API functions and proxies them to the remote device


# Important

The injected DLL will reload VMM.DLL. Thus you need to place it into a folder inside your PATH env var. E.g. C:\Windows\System32