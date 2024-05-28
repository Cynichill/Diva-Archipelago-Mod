//MIT License
//
//Copyright(c) 2022 Skyth
//
//Permission is hereby granted, free of charge, to any person obtaining a copy of this softwareand associated documentation files(the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and /or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions :
//
//The above copyright noticeand this permission notice shall be included in all copies or substantial portions of the Software.
//
//THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#include <windows.h>
#include "MemSigScan.h"
#include <Psapi.h>
#include "pch.h"

extern HMODULE hm;

MODULEINFO moduleInfoMem;

const MODULEINFO& getModuleInfoMem()
{
    if (moduleInfoMem.SizeOfImage)
        return moduleInfoMem;

    ZeroMemory(&moduleInfoMem, sizeof(MODULEINFO));
    GetModuleInformation(GetCurrentProcess(), GetModuleHandle(nullptr), &moduleInfoMem, sizeof(MODULEINFO));

    return moduleInfoMem;
}

FORCEINLINE void* memSigScan(const char* signature, const char* mask, size_t sigSize, void* memory, const size_t memorySize)
{
    if (sigSize == 0)
        sigSize = strlen(mask);

    for (size_t i = 0; i < memorySize; i++)
    {
        char* currMemory = (char*)memory + i;

        size_t j;
        for (j = 0; j < sigSize; j++)
        {
            if (mask[j] != '?' && signature[j] != currMemory[j])
                break;
        }

        if (j == sigSize)
        {
            return currMemory;
        }
    }

    return nullptr;
}

FORCEINLINE void* memSigScan(const char* signature, const char* mask, void* hint)
{
    const MODULEINFO& info = getModuleInfoMem();
    const size_t sigSize = strlen(mask);

    // Ensure hint address is within the process memory region so there are no crashes.
    if ((hint >= info.lpBaseOfDll) && ((char*)hint + sigSize <= (char*)info.lpBaseOfDll + info.SizeOfImage))
    {
        void* result = memSigScan(signature, mask, sigSize, hint, sigSize);

        if (result)
            return result;
    }

    return nullptr;
}
