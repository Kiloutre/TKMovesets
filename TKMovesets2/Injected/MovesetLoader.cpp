#include <thread>
#include <stdint.h>
#include <string>

#include "MovesetLoader.hpp"
#include "MovesetLoader_t7.hpp"

#include "constants.h"

// Store info globally that way repeated calls to MovesetLoaderStart() won't start multiple loaders
HINSTANCE hModule = nullptr;
MovesetLoader* g_loader = nullptr;
bool g_loading = false;

// -- Debug console -- //
// Only if compiled in debug mode

void InitConsole()
{
    // Create console
    AllocConsole();
    // Redirect stdout to console
    freopen("CONOUT$", "w", stdout);

    DEBUG_LOG("-- Console started --\n");
}

// -- Static helpers -- //

static std::string GetModuleFilenameStr()
{
    TCHAR szFileName[MAX_PATH];

    GetModuleFileName(NULL, szFileName, MAX_PATH);
    std::string filename = std::string(szFileName);
    filename.erase(0, filename.find_last_of("\\/") + 1);
    return filename;
}

//  -- Moveset Loader -- //

MovesetLoader::~MovesetLoader()
{
    if (m_memoryHandle != nullptr) {
        CloseHandle(m_memoryHandle);
    }

    // Unhook any previously hooked function
    for (auto& [name, hook] : m_hooks)
    {
        hook.detour->unHook();
        delete hook.detour;
    }

    m_hooks.clear();
    delete addresses.addrFile;
}

bool MovesetLoader::Init()
{
    SetAddressesGameKey();
    addresses.addrFile = new GameAddressesFile;

    // Initialize the list of hooks and error out if a required one is not found
    {
        InitHooks();

        bool requiredHookErr = false;
        for (auto& name : m_requiredSymbols)
        {
            if (!m_hooks.contains(name) && (!m_functions.contains(name) || m_functions[name] == 0)
                && (!variables.contains(name) || variables[name] == 0)) {
                DEBUG_LOG("Error: failed to find required function '%s()'\n", name.c_str());
                requiredHookErr = true;
            }
        }
        if (requiredHookErr) {
            return false;
        }
    }

    // Load the shared memory handle
    auto sharedMemName = GetSharedMemoryName();
    DEBUG_LOG("Shared memory name is '%s'\n", sharedMemName);

    m_memoryHandle = CreateFileMapping(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, SHARED_MEMORY_BUFSIZE, sharedMemName);
    DEBUG_LOG("Shared memory handle is %llx\n", (unsigned long long)m_memoryHandle);
    if (m_memoryHandle == nullptr) {
        return false;
    }

    orig_sharedMemPtr = (void*)MapViewOfFile(m_memoryHandle, FILE_MAP_ALL_ACCESS, 0, 0, SHARED_MEMORY_BUFSIZE);
    DEBUG_LOG("Shared memory address: %p\n", orig_sharedMemPtr);
    if (orig_sharedMemPtr == nullptr)
    {
        CloseHandle(m_memoryHandle);
        m_memoryHandle = nullptr;
        return false;
    }
    memset(orig_sharedMemPtr, 0, SHARED_MEMORY_BUFSIZE);

    PostInit();
    return true;
}

void MovesetLoader::InitHook(const char* hookName, uint64_t originalAddr, uint64_t newAddr)
{
    if (originalAddr == 0) {
        DEBUG_LOG("Could not find function '%s'\n", hookName);
        return ;
    }

    m_hooks[hookName] = {};
    auto& hook = m_hooks[hookName];

    // Initialize the original function's hook
    hook.detour = new PLH::x64Detour(originalAddr, newAddr, &hook.trampoline, m_disassembler);
}

void MovesetLoader::SetMainModule(const char* name)
{
    m_moduleName = name;
    m_modules = InjectionUtils::GetModuleList(name, m_moduleAddr, m_moduleSize);
}

void MovesetLoader::RegisterFunction(const char* functionName, const std::string& moduleName, const char* c_addressId)
{
    auto mod = m_modules.find(moduleName);
    uint64_t moduleAddr;
    uint64_t moduleSize;

    if (mod == m_modules.end()) {
        DEBUG_LOG("RegisterFunction(%s, %s, %s) : Could not find module\n", functionName, moduleName, c_addressId);
        return;
    }
    else {
        moduleAddr = mod->second.address;
        moduleSize = mod->second.size;
    }

    const char* functionBytes = addresses.GetString(c_addressId);
    m_functions[functionName] = (uint64_t)Sig::find((void*)moduleAddr, moduleSize, functionBytes);
    DEBUG_LOG("RegisterFunction(%s): Found function at %llx (+%llx)\n", functionName, m_functions[functionName], m_functions[functionName] - moduleAddr);
}

void MovesetLoader::RegisterHook(const char* functionName, const std::string& moduleName, const char* c_addressId, uint64_t hookAddr)
{
    auto mod = m_modules.find(moduleName);
    uint64_t moduleAddr;
    uint64_t moduleSize;

    if (mod == m_modules.end()) {
        DEBUG_LOG("RegisterHook(%s, %s, %s, ?) : Could not find module\n", functionName, moduleName, c_addressId);
        return;
    }
    else {
        moduleAddr = mod->second.address;
        moduleSize = mod->second.size;
    }

    const char* functionBytes = addresses.GetString(c_addressId);
    uint64_t functionAddr = (uint64_t)Sig::find((void*)moduleAddr, moduleSize, functionBytes);
    InitHook(functionName, functionAddr, hookAddr);
    DEBUG_LOG("RegisterHook(%s): Found function at %llx (+%llx)\n", functionName, functionAddr, functionAddr - moduleAddr);
}

uint64_t MovesetLoader::GetFunctionAddr(const char* functionName)
{
    auto func = m_functions.find(functionName);
    if (func != m_functions.end()) {
        return func->second;
    }
    DEBUG_LOG("GetFunctionAddr(): Function '%s' not found\n", functionName.c_str());
    return 0;
}

void MovesetLoader::HookFunction(const char* functionName)
{
    auto hook = m_hooks.find(functionName);
    if (hook != m_hooks.end()) {
        hook->second.detour->hook();
    }
}

// -- Static helper -- //

static bool Init()
{
    // Safety in case this function is called multiple times
    if (g_loader != nullptr && g_loader->isInitialized()) {
        DEBUG_LOG("Loader already started: not starting again\n");
        return false;
    }
    if (g_loading) {
        DEBUG_LOG("Loader already starting...\n");
        return false;
    }

    g_loading = true;
    // Display console if debug mode
#ifdef BUILD_TYPE_DEBUG
    {
        InitConsole();
        DEBUG_LOG("MovesetLoaderStart\n");
    }
#endif

    // Most likely g_loader is already null, but if previous initialization has failed, it won't be
    if (g_loader == nullptr)
    {
        std::string processName = GetModuleFilenameStr();
        if (processName == "TekkenGame-Win64-Shipping.exe") {
            g_loader = new MovesetLoaderT7;
        }
        else {
            g_loading = false;
            return false;
        }
        g_loading = false;
        g_loader->SetMainModule(processName.c_str());
    }

    // Start loop only if Init() succeeds
    // Mainloop will run until process is closed or if MovesetLoaderStop() is called externally
    if (!g_loader->Init()) {
        DEBUG_LOG("Error during MovesetLoader::Init()\n");
        delete g_loader;
        g_loader = nullptr;
        return false;
    }
    return true;
}

static void Run()
{
    DEBUG_LOG("Starting mainloop.\n");
    g_loader->Mainloop();
    DEBUG_LOG("Mainloop stopped. Unloading the DLL.\n");
    // If mainloop is stopped then we have no reason to let the DLL stay loaded
    delete g_loader;
    FreeLibraryAndExitThread(hModule, 0);
}


// -- DLL Exported functions -- //

extern "C"
{
    bool __declspec(dllexport) MovesetLoaderInit()
    {
        DEBUG_LOG("MovesetLoaderInit\n");
        bool res = Init();
        DEBUG_LOG("MovesetLoaderInit() -> return(%d)\n", res);
        return res;
    }

    void __declspec(dllexport) MovesetLoaderRun()
    {
        DEBUG_LOG("MovesetLoaderRun\n");
        Run();
        DEBUG_LOG("MovesetLoaderRun() -> return\n");
    }

    void __declspec(dllexport) MovesetLoaderStop()
    {
        // Clean way of stopping the loader
        DEBUG_LOG("MovesetLoaderStop\n");
        if (g_loader != nullptr) {
            g_loader->mustStop = true;
        }
        DEBUG_LOG("MovesetLoaderStop() -> return\n");
    }

    void __declspec(dllexport) ExecuteExtraprop()
    {
        if (g_loader != nullptr) {
            g_loader->ExecuteExtraprop();
        }
    }
}

// DLLMAIN

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    switch (fdwReason)
    {
    case DLL_PROCESS_ATTACH:
        hModule = hinstDLL;
        break;

    case DLL_THREAD_ATTACH:
        break;

    case DLL_THREAD_DETACH:
        break;

    case DLL_PROCESS_DETACH: 
        if (lpvReserved != nullptr) {
            break;
        }
        break;
    }
    return TRUE;
}