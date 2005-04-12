#ifdef _WIN32

#include <windows.h>

#define MAX_SEM_COUNT 1024 // What are the tradeoffs with this value?

void* naNewLock()
{
    LPCRITICAL_SECTION lock = malloc(sizeof(CRITICAL_SECTION));
    InitializeCriticalSection(lock);
    return lock;
}

void  naLock(void* lock)   { EnterCriticalSection((LPCRITICAL_SECTION)lock); }
void  naUnlock(void* lock) { LeaveCriticalSection((LPCRITICAL_SECTION)lock); }
void* naNewSem()           { return CreateSemaphore(0, 0, MAX_SEM_COUNT, 0); }
void  naFreeSem(void* sh)  { CloseHandle((HANDLE)sh); }
void  naSemDown(void* sem) { WaitForSingleObject((HANDLE)sem, INFINITE); }
void  naSemUp(void* sem)   { ReleaseSemaphore(sem, 1, 0); }
void  naSemUpAll(void* sem, int count) { ReleaseSemaphore(sem, count, 0); }

#endif
