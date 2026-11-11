#include "Zip/ZipManager.h"
#include "Zip/zip.h"
#include "Type/NativeTypeIdentity.h"
#include "Misc/ScopeExit.h"

#include "HAL/FileManager.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/Paths.h"
#include "Async/Async.h"
#include "Main/MainManager.h"
#if PLATFORM_WINDOWS
#include <windows.h>
#endif

#define ZIP_BUFFER_MAX 1024

namespace
{
bool ZipDestinationsOverlap(const FString& A, const FString& B)
{
    return FPaths::IsSamePath(A, B) || FPaths::IsUnderDirectory(A, B) || FPaths::IsUnderDirectory(B, A);
}
}

const FUniqueType FZipManager::Type = FUniqueType(&FManagerBase::Type);

IMPLEMENTATION_MANAGER(FZipManager)

FZipManager::FZipManager() : FManagerBase(Type)
{
}

FZipManager::~FZipManager()
{
    OnTermination();
}

void FZipManager::RetireCompletedTasks()
{
    RunningTasks.RemoveAll([](const FRunningTask& Task) { return Task.Future.IsReady(); });
}

void FZipManager::OnTermination()
{
    if (bStopping) return;
    bStopping = true;
    for (FRunningTask& Task : RunningTasks) Task.Handle->Cancel();
    // The futures signal only after native I/O and the archive are closed.
    // A library or filesystem call is not interruptible and can delay shutdown.
    for (FRunningTask& Task : RunningTasks) Task.Future.Wait();
    RunningTasks.Empty();
    FManagerBase::OnTermination();
}

TSharedPtr<FZipTask, ESPMode::ThreadSafe> FZipManager::StartPack(const FString& InZipFile, const FString& InOriginDirectory, const FString& InIgnoreFile)
{
    if (!IsInGameThread() || bStopping) return nullptr;
    RetireCompletedTasks();
    const FString Destination = FPaths::ConvertRelativePathToFull(InZipFile);
    const FString Origin = FPaths::ConvertRelativePathToFull(InOriginDirectory);
    const FString Ignored = InIgnoreFile.IsEmpty() ? FString() : FPaths::ConvertRelativePathToFull(InIgnoreFile);
    if (RunningTasks.ContainsByPredicate([&](const FRunningTask& Active) { return ZipDestinationsOverlap(Active.Destination, Destination); })) return nullptr;
    TSharedRef<FZipTask, ESPMode::ThreadSafe> Task = MakeShared<FZipTask, ESPMode::ThreadSafe>();
    FRunningTask& Running = RunningTasks.AddDefaulted_GetRef();
    Running.Handle = Task;
    Running.Destination = Destination;
    Running.Future = Async(EAsyncExecution::ThreadPool, [Task, Destination, Origin, Ignored]()
    {
        Task->Status.store(EZipTaskStatus::Running);
        bool bCancelled = false;
        const bool bSucceeded = Pack(Destination, Origin, Ignored, *Task, bCancelled);
        if (!bSucceeded && !bCancelled) Task->FailureMessage = FString::Printf(TEXT("Failed to pack %s into %s."), *Origin, *Destination);
        Task->Status.store(bSucceeded ? EZipTaskStatus::Succeeded : bCancelled ? EZipTaskStatus::Cancelled : EZipTaskStatus::Failed);
    });
    return Task;
}

TSharedPtr<FZipTask, ESPMode::ThreadSafe> FZipManager::StartUnpack(const FString& InZipFile, const FString& InDestDirectory)
{
    if (!IsInGameThread() || bStopping) return nullptr;
    RetireCompletedTasks();
    const FString Destination = FPaths::ConvertRelativePathToFull(InDestDirectory);
    const FString Archive = FPaths::ConvertRelativePathToFull(InZipFile);
    if (RunningTasks.ContainsByPredicate([&](const FRunningTask& Active) { return ZipDestinationsOverlap(Active.Destination, Destination); })) return nullptr;
    TSharedRef<FZipTask, ESPMode::ThreadSafe> Task = MakeShared<FZipTask, ESPMode::ThreadSafe>();
    FRunningTask& Running = RunningTasks.AddDefaulted_GetRef();
    Running.Handle = Task;
    Running.Destination = Destination;
    Running.Future = Async(EAsyncExecution::ThreadPool, [Task, Archive, Destination]()
    {
        Task->Status.store(EZipTaskStatus::Running);
        bool bCancelled = false;
        const bool bSucceeded = Unpack(Archive, Destination, *Task, bCancelled);
        if (!bSucceeded && !bCancelled) Task->FailureMessage = FString::Printf(TEXT("Failed to extract %s into %s; destination may be partially updated."), *Archive, *Destination);
        Task->Status.store(bSucceeded ? EZipTaskStatus::Succeeded : bCancelled ? EZipTaskStatus::Cancelled : EZipTaskStatus::Failed);
    });
    return Task;
}

bool FZipManager::Pack(const FString& InZipFile, const FString& InOriginDirectory, const FString& InIgnoreFile, FZipTask& InTask, bool& bOutCancelled)
{
    IPlatformFile& Platform = FPlatformFileManager::Get().GetPlatformFile();
    if (InTask.IsCancellationRequested()) { bOutCancelled = true; return false; }
    if (Platform.FileExists(*InZipFile)) return false;
    int Error = 0;
    zip* Archive = zip_open(TCHAR_TO_UTF8(*InZipFile), ZIP_CREATE | ZIP_EXCL, &Error);
    if (!Archive) return false;
    const FString Directory = InOriginDirectory / FString();
    if (!Pack(Archive, Directory, Directory, InIgnoreFile, InTask, bOutCancelled))
    {
        zip_discard(Archive);
        return false;
    }
    if (InTask.IsCancellationRequested())
    {
        bOutCancelled = true;
        zip_discard(Archive);
        return false;
    }
    if (zip_close(Archive) != 0)
    {
        zip_discard(Archive);
        return false;
    }
    return true;
}

bool FZipManager::Pack(zip* InArchive, const FString& InDirectory, const FString& InRelativePath, const FString& InIgnoreFile, FZipTask& InTask, bool& bOutCancelled)
{
    static IFileManager& fm = IFileManager::Get();
    if (!fm.DirectoryExists(*InDirectory))
    {
        UE_LOG(LogTemp, Error, TEXT("Zip source directory does not exist: %s"), *InDirectory);
        return false;
    }
    TArray<FString> files;
    const FString dir = InDirectory / TEXT("*");
    fm.FindFiles(files, *dir, true, true);
    for (FString f : files)
    {
        if (InTask.IsCancellationRequested()) { bOutCancelled = true; return false; }
        FString p = InDirectory / f;
        if (!InIgnoreFile.IsEmpty() && FPaths::IsSamePath(p, InIgnoreFile)) continue;
        if (fm.DirectoryExists(*p))
        {
            if (!Pack(InArchive, p, InRelativePath, InIgnoreFile, InTask, bOutCancelled))
            {
                return false;
            }
        }
        else
        {
            FString z = p;
            FPaths::MakePathRelativeTo(z, *InRelativePath);
#if PLATFORM_WINDOWS
            p = FPaths::ConvertRelativePathToFull(p);
#endif
            zip_source_t* Source = zip_source_file(InArchive, TCHAR_TO_UTF8(*p), 0, -1);
            if (!Source || zip_add(InArchive, TCHAR_TO_UTF8(*z), Source) == -1)
            {
                if (Source) zip_source_free(Source);
                UE_LOG(LogTemp, Error, TEXT("Zip entry could not be added: source=%s entry=%s"), *p, *z);
                return false;
            }
        }
    }
    return true;
}

bool FZipManager::Unpack(const FString& InZipFile, const FString& InDestDirectory, FZipTask& InTask, bool& bOutCancelled)
{
    IPlatformFile& Platform = FPlatformFileManager::Get().GetPlatformFile();
    if (InTask.IsCancellationRequested()) { bOutCancelled = true; return false; }
    if (!Platform.FileExists(*InZipFile)) return false;
    if (!Platform.DirectoryExists(*InDestDirectory) && !Platform.CreateDirectoryTree(*InDestDirectory)) return false;
    int Error = 0;
    zip* Archive = zip_open(TCHAR_TO_UTF8(*InZipFile), ZIP_RDONLY, &Error);
    if (!Archive) return false;
    ON_SCOPE_EXIT { zip_discard(Archive); };
    FString Root = FPaths::ConvertRelativePathToFull(InDestDirectory);
    FPaths::NormalizeDirectoryName(Root);
    const int64 Count = zip_get_num_entries(Archive, 0);
    if (Count < 0) return false;
    TArray<uint8> Buffer;
    Buffer.SetNumUninitialized(ZIP_BUFFER_MAX);
    for (int64 Index = 0; Index < Count; ++Index)
    {
        if (InTask.IsCancellationRequested()) { bOutCancelled = true; return false; }
        const char* Name = zip_get_name(Archive, Index, ZIP_FL_ENC_RAW);
        if (!Name) return false;
        const FString Relative = IsUTF8(Name) ? UTF8_TO_TCHAR(Name) : ANSIToUTF8ToFString(Name);
        if (Relative.IsEmpty() || !FPaths::IsRelative(Relative)) return false;
        FString Destination = FPaths::ConvertRelativePathToFull(Root / Relative);
        FPaths::NormalizeFilename(Destination);
        if (!FPaths::CollapseRelativeDirectories(Destination) || !FPaths::IsUnderDirectory(Destination, Root)) return false;
        if (Relative.EndsWith(TEXT("/")) || Relative.EndsWith(TEXT("\\")))
        {
            if (!Platform.DirectoryExists(*Destination) && !Platform.CreateDirectoryTree(*Destination)) return false;
            continue;
        }
        const FString Parent = FPaths::GetPath(Destination);
        if (!Platform.DirectoryExists(*Parent) && !Platform.CreateDirectoryTree(*Parent)) return false;
        zip_file* File = zip_fopen_index(Archive, Index, ZIP_FL_UNCHANGED);
        if (!File) return false;
        ON_SCOPE_EXIT { if (File) zip_fclose(File); };
        TUniquePtr<FArchive> Writer(IFileManager::Get().CreateFileWriter(*Destination, FILEWRITE_None));
        if (!Writer) return false;
        zip_int64_t Length = 0;
        while ((Length = zip_fread(File, Buffer.GetData(), Buffer.Num())) > 0)
        {
            if (InTask.IsCancellationRequested()) { bOutCancelled = true; return false; }
            Writer->Serialize(Buffer.GetData(), Length);
            if (Writer->IsError()) return false;
        }
        if (Length < 0 || !Writer->Close()) return false;
        const int CloseResult = zip_fclose(File);
        File = nullptr;
        if (CloseResult != 0) return false;
    }
    return true;
}

bool FZipManager::IsZip(const FString& InZipFile)
{
    int e = 0;
    zip* z = zip_open(TCHAR_TO_UTF8(*InZipFile), 0, &e);
    if (!z)
    {
        return false;
    }
    zip_close(z);
    return true;
}

bool FZipManager::IsUTF8(const char* InChars)
{
    unsigned int nBytes = 0;    // UFT8可用1-6个字节编码,ASCII用一个字节
    unsigned char chr = *InChars;
    bool bAllAscii = true;
    for (unsigned int i = 0; InChars[i] != '\0'; ++i)
    {
        chr = *(InChars + i);
        //判断是否ASCII编码,如果不是,说明有可能是UTF8,ASCII用7位编码,最高位标记为0,0xxxxxxx
        if (nBytes == 0 && (chr & 0x80) != 0)
        {
            bAllAscii = false;
        }
        if (nBytes == 0)
        {
            //如果不是ASCII码,应该是多字节符,计算字节数
            if (chr >= 0x80)
            {
                if (chr >= 0xFC && chr <= 0xFD)
                {
                    nBytes = 6;
                }
                else if (chr >= 0xF8)
                {
                    nBytes = 5;
                }
                else if (chr >= 0xF0)
                {
                    nBytes = 4;
                }
                else if (chr >= 0xE0)
                {
                    nBytes = 3;
                }
                else if (chr >= 0xC0)
                {
                    nBytes = 2;
                }
                else
                {
                    return false;
                }
                nBytes--;
            }
        }
        else
        {
            //多字节符的非首字节,应为 10xxxxxx
            if ((chr & 0xC0) != 0x80)
            {
                return false;
            }
            //减到为零为止
            nBytes--;
        }
    }
    //违返UTF8编码规则
    if (nBytes != 0)
    {
        return false;
    }
    if (bAllAscii)
    {
        //如果全部都是ASCII, 也是UTF8
        return true;
    }
    return true;
}

FString FZipManager::ANSIToUTF8ToFString(const char* InChars)
{
#if PLATFORM_WINDOWS
    if (InChars == nullptr || strlen(InChars) == 0)
    {
        return FString();
    }
    int l = MultiByteToWideChar(CP_ACP, 0, InChars, -1, nullptr, 0);
    wchar_t* wc = new (std::nothrow) wchar_t[l];
    if (wc == nullptr)
    {
        delete[] wc;
        return FString();
    }
    wmemset(wc, 0, l);
    MultiByteToWideChar(CP_ACP, 0, InChars, -1, wc, l);
    if (wc == nullptr || wcslen(wc) == 0)
    {
        delete[] wc;
        return FString();
    }
    l = WideCharToMultiByte(CP_UTF8, 0, wc, -1, nullptr, 0, nullptr, nullptr);
    char* c = new (std::nothrow) char[l];
    if (c == nullptr)
    {
        delete[] wc;
        delete[] c;
        return FString();
    }
    memset(c, 0, l);
    WideCharToMultiByte(CP_UTF8, 0, wc, -1, c, l, nullptr, nullptr);
    FString str(UTF8_TO_TCHAR(c));
    delete[] wc;
    delete[] c;
    return str;
#else
    return ANSI_TO_TCHAR(InChars);
#endif
}
