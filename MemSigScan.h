#pragma once
#include <cstdint>

// Signature scan in specified memory region
extern void* memSigScan(const char* signature, const char* mask, size_t sigSize, void* memory, size_t memorySize);

// Signature scan in current process
extern void* memSigScan(const char* signature, const char* mask, void* hint = nullptr);
