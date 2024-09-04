#include "Zip/ZipManager.h"

#include "HAL/FileManager.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/Paths.h"
#include "Async/Async.h"
#include "Debug/DebugTypes.h"
#include "Main/MainManager.h"
#if PLATFORM_WINDOWS
#include <windows.h>
#endif

#define ZIP_THREAD_MAX 10
#define ZIP_BUFFER_MAX 1024

const FUniqueType FZipManager::Type = FUniqueType(&FManagerBase::Type);

IMPLEMENTATION_MANAGER(FZipManager)

FZipManager::FZipManager() : FManagerBase(Type)
{
}

FZipManager::~FZipManager()
{
}

bool FZipManager::Pack(const FString& InZipFile, const FString& InOriginDirectory, const FString& InIgnoreFile)
{
    static IPlatformFile& pf = FPlatformFileManager::Get().GetPlatformFile();
    if (pf.FileExists(*InZipFile))
    {
        WHLog(FString::Printf(TEXT("zip pack failed! zip path already exist! %s"), *InZipFile), EDC_Zip, EDV_Error);
        return false;
    }
    int err;
    zip* Archive = zip_open(TCHAR_TO_UTF8(*InZipFile), ZIP_CREATE, &err);
    if (!Archive)
    {
        WHLog(FString::Printf(TEXT("zip pack failed! zip_open failed(%d): %s"), err, *InZipFile), EDC_Zip, EDV_Error);
        return false;
    }
    const FString d = InOriginDirectory / FString();
    TArray<TSharedPtr<EStatus>> ss;
    bool r = Pack(Archive, d, d, ss);
    Recheck:
        for(auto v : ss)
        {
            if(*v.Get() == EStatus::Doing)
            {
                FPlatformProcess::SleepNoStats(0.03f);
                goto Recheck;
            }
        }
    for(auto v : ss)
    {
        if(*v.Get() == EStatus::Failed)
        {
            //有异步执行失败
            r = false;
        }
    }
    err = zip_close(Archive);
    if (err != 0)
    {
        WHLog(FString::Printf(TEXT("zip pack warning! zip_close failed(%d): %s"), err, *InZipFile), EDC_Zip, EDV_Error);
        //return false; //暂时不处理关闭问题
    }
    return r;
}

bool FZipManager::Unpack(const FString& InZipFile, const FString& InDestDirectory)
{
    TRACE_CPUPROFILER_EVENT_SCOPE_TEXT("MW_Uzip_Unpack")
    static IPlatformFile& pf = FPlatformFileManager::Get().GetPlatformFile();
    if(!pf.FileExists(*InZipFile))
    {
        WHLog(FString::Printf(TEXT("zip unpack failed! zipfile(%s) not exist!"), *InZipFile), EDC_Zip, EDV_Error);
        return false;
    }
    if (!pf.DirectoryExists(*InDestDirectory))
    {
        pf.CreateDirectoryTree(*InDestDirectory);
    }
#if 1//UE_SERVER //Linux下可能需要Root权限才能申请到线程。否则报 invalid attempt to read memory at address 0x0000000000000000
    return Unpack(InZipFile, InDestDirectory, 0, 1);
#else//暂时关闭多线程解压,部分手机结果不理想
    TArray<TSharedPtr<EStatus>> ss;
    for (int i = 0; i < ZIP_THREAD_MAX; i++)
    {
        TSharedPtr<EStatus> h = MakeShared<EStatus>();
        ss.Add(h);
        Async(EAsyncExecution::ThreadPool, [=]()
        {
            if (Unpack(InZipFile, InDestDirectory, i,ZIP_THREAD_MAX))
            {
                *h.Get() = EStatus::Succeeded;
            }
            else
            {
                *h.Get() = EStatus::Failed;
            }
        });
    }
    Recheck:
        for (const auto& s : ss)
        {
            if (*s.Get() == EStatus::Doing)
            {
                FPlatformProcess::SleepNoStats(0.03f);
                goto Recheck;
            }
        }
    for (const auto& s : ss)
    {
        if (*s.Get() == EStatus::Failed)
        {
            return false;
        }
    }
    return true;
#endif
}

bool FZipManager::Pack(const FString& InZipFile, const TArray<FString>& InOriginFiles, const FString& InRootDirectory)
{
    static IFileManager& fm = IFileManager::Get();
    if(fm.FileExists(*InZipFile))
    {
        WHLog(FString::Printf(TEXT("zip create failed: zip path already exist! %s"), *InZipFile), EDC_Zip, EDV_Error);
        return false;
    }
    int Err;
    zip* Archive = zip_open(TCHAR_TO_UTF8(*InZipFile), ZIP_CREATE, &Err);
    if (!Archive)
    {
        WHLog(FString::Printf(TEXT("zip open failed(%d): %s"), Err, *InZipFile), EDC_Zip, EDV_Error);
        return false;
    }
    for(FString p : InOriginFiles)
    {
#if PLATFORM_WINDOWS
        p = FPaths::ConvertRelativePathToFull(p);
#endif
        zip_source_t* Source = zip_source_file(Archive, TCHAR_TO_UTF8(*p), 0, -1);
        FString z;
        if (InRootDirectory.IsEmpty())
        {
            z = FPaths::GetBaseFilename(p, false);
        }
        else
        {
            z = p;
            FPaths::MakePathRelativeTo(z, *InRootDirectory);
        }
        if (zip_add(Archive, TCHAR_TO_UTF8(*z), Source) == -1)
        {
            zip_source_free(Source);
            WHLog(FString::Printf(TEXT("zip add entry failed: source_file:%s zip_file:%s"), *p, *z), EDC_Zip, EDV_Error);
            return false;
        }
    }
    return true;
}

bool FZipManager::Pack(zip* InArchive, const FString& InDirectory, const FString& InRelativePath, TArray<TSharedPtr<EStatus>>& InAsyncStatuses)
{
    static IFileManager& fm = IFileManager::Get();
    if (!fm.DirectoryExists(*InDirectory))
    {
        WHLog(FString::Printf(TEXT("zip Add failed, directory not exist: %s"), *InDirectory), EDC_Zip, EDV_Error);
        return false;
    }
    TArray<FString> files;
    const FString dir = InDirectory / TEXT("*");
    const FString fd = FPaths::GetPath(InRelativePath);
    fm.FindFiles(files, *dir, true, true);
    for (FString f : files)
    {
        FString p = InDirectory / f;
        if (fm.DirectoryExists(*p))
        {
            if (!Pack(InArchive, p, InRelativePath, InAsyncStatuses))
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
            if (zip_add(InArchive, TCHAR_TO_UTF8(*z), Source) == -1)
            {
                zip_source_free(Source);
                zip_close(InArchive);
                WHLog(FString::Printf(TEXT("zip add failed: source_file:%s zip_file:%s"), *p, *z), EDC_Zip, EDV_Error);
                return false;
            }
        }
    }
    return true;
}

bool FZipManager::Unpack(const FString& InZipFile, const FString& InDestDirectory, int InIndex, int InStep)
{
    static IPlatformFile& pf = FPlatformFileManager::Get().GetPlatformFile();
    int err = 0;
    zip* z = zip_open(TCHAR_TO_UTF8(*InZipFile), 0, &err);
    if (!z)
    {
        WHLog(FString::Printf(TEXT("zip unpack failed! open zip(%s) err(%d)"),*InZipFile, err), EDC_Zip, EDV_Error);
        return false;
    }
    const int num = zip_get_num_files(z);
    char* b = new char[ZIP_BUFFER_MAX];
    for (int i = InIndex; i < num; i += InStep)
    {
        const char* n = zip_get_name(z, i, ZIP_FL_ENC_RAW);
        FString d = InDestDirectory / (IsUTF8(n) ? UTF8_TO_TCHAR(n) : ANSIToUTF8ToFString(n));
        if(d.EndsWith("/") || d.EndsWith("\\"))
        {
            if (!pf.DirectoryExists(*d))
            {
                pf.CreateDirectoryTree(*d);
            }
            continue;
        }
        zip_file* f = zip_fopen_index(z, i, ZIP_FL_UNCHANGED);
        if (!f)
        {
            WHLog(FString::Printf(TEXT("zip unpack failed! zip file open(%hs) failed!"), n), EDC_Zip, EDV_Error);
            continue;
        }
        TUniquePtr<FArchive> w(IFileManager::Get().CreateFileWriter(*d, FILEWRITE_None));
        if(!w.IsValid())
        {
            WHLog(FString::Printf(TEXT("zip unpack failed! createfilewriter(%s) failed!"), *d), EDC_Zip, EDV_Error);
            continue;
        }
        zip_int64_t l = zip_fread(f, b, ZIP_BUFFER_MAX);
        while (l > 0)
        {
            w->Serialize(b, l);
            l = zip_fread(f, b, ZIP_BUFFER_MAX);
        }
        if (l < 0)
        {
            WHLog(FString::Printf(TEXT("zip unpack failed! zip file read(%hs) failed!"), n), EDC_Zip, EDV_Error);
        }
        w->Close();
        zip_fclose(f);
    }
    delete[] b;
    zip_close(z);
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
