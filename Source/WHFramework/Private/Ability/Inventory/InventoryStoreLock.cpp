#include "Ability/Inventory/InventoryStoreLock.h"

#include "HAL/PlatformFileManager.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#elif PLATFORM_LINUX
#include <cerrno>
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#endif

int64 FInventoryStoreLock::Acquire(const FString& InDirectory, FString& OutError)
{
    FString Directory = FPaths::ConvertRelativePathToFull(InDirectory);
    FPaths::NormalizeDirectoryName(Directory);
    const FString MarkerPath = Directory / TEXT("store.lock");
#if PLATFORM_WINDOWS
    FString NativeDirectory = FPlatformFileManager::Get().GetPlatformFile()
        .ConvertToAbsolutePathForExternalAppForWrite(*Directory);
    FPaths::MakePlatformFilename(NativeDirectory);
    if (!NativeDirectory.StartsWith(TEXT("\\\\?\\")))
    {
        NativeDirectory = NativeDirectory.StartsWith(TEXT("\\\\"))
            ? TEXT("\\\\?\\UNC\\") + NativeDirectory.Mid(2) : TEXT("\\\\?\\") + NativeDirectory;
    }
    const HANDLE DirectoryHandle = ::CreateFileW(*NativeDirectory, 0,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (DirectoryHandle == INVALID_HANDLE_VALUE)
    {
        OutError = TEXT("Inventory.StoreLockDirectoryUnavailable");
        return -1;
    }
    // Resolve junctions, case variants and drive aliases to one physical directory.
    TCHAR FinalPath[32768];
    const DWORD Length = ::GetFinalPathNameByHandleW(DirectoryHandle, FinalPath, UE_ARRAY_COUNT(FinalPath),
        FILE_NAME_NORMALIZED | VOLUME_NAME_GUID);
    ::CloseHandle(DirectoryHandle);
    if (Length == 0 || Length >= UE_ARRAY_COUNT(FinalPath))
    {
        OutError = TEXT("Inventory.StoreLockPhysicalPathUnavailable");
        return -1;
    }
    const FString Key = FString(FinalPath).ToLower();
    const FTCHARToUTF8 KeyBytes(*Key);
    uint8 Digest[FSHA1::DigestSize];
    FSHA1::HashBuffer(KeyBytes.Get(), KeyBytes.Length(), Digest);
    const FString Name = TEXT("Global\\WHFramework.InventoryStore.") + BytesToHex(Digest, UE_ARRAY_COUNT(Digest));
    // Existence owns the lease; a second CreateEvent is rejected even on the same thread.
    // CloseHandle is thread-independent, and process exit releases the kernel handle.
    const HANDLE Handle = ::CreateEventW(nullptr, true, false, *Name);
    const DWORD NativeError = ::GetLastError();
    if (!Handle || NativeError == ERROR_ALREADY_EXISTS)
    {
        if (Handle) ::CloseHandle(Handle);
        OutError = TEXT("Inventory.StoreAlreadyOpenOrLockUnavailable");
        return -1;
    }
    // Keep a discovery marker without an open child-file handle.
    IPlatformFile& Platform = FPlatformFileManager::Get().GetPlatformFile();
    if (!Platform.FileExists(*MarkerPath))
    {
        TUniquePtr<IFileHandle> Marker(Platform.OpenWrite(*MarkerPath));
        if (!Marker || !Marker->Flush(true))
        {
            ::CloseHandle(Handle);
            OutError = TEXT("Inventory.StoreLockMarkerUnavailable");
            return -1;
        }
    }
    return static_cast<int64>(reinterpret_cast<UPTRINT>(Handle));
#elif PLATFORM_LINUX
    const FTCHARToUTF8 Path(*MarkerPath);
    const int Descriptor = ::open(Path.Get(), O_CREAT | O_RDWR | O_CLOEXEC, 0600);
    if (Descriptor < 0)
    {
        OutError = TEXT("Inventory.StoreLockOpenFailed");
        return -1;
    }
    int Result;
    do { Result = ::flock(Descriptor, LOCK_EX | LOCK_NB); } while (Result < 0 && errno == EINTR);
    if (Result < 0)
    {
        ::close(Descriptor);
        OutError = TEXT("Inventory.StoreAlreadyOpenOrLockUnavailable");
        return -1;
    }
    return Descriptor;
#else
    OutError = TEXT("Inventory.UnsupportedDurabilityPlatform");
    return -1;
#endif
}

void FInventoryStoreLock::Release(int64 InHandle)
{
    if (InHandle == -1) return;
#if PLATFORM_WINDOWS
    ::CloseHandle(reinterpret_cast<HANDLE>(static_cast<UPTRINT>(InHandle)));
#elif PLATFORM_LINUX
    ::flock(static_cast<int>(InHandle), LOCK_UN);
    ::close(static_cast<int>(InHandle));
#endif
}
