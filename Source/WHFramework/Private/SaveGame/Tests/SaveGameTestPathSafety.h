#pragma once

#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/Char.h"
#include "Misc/Paths.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#elif PLATFORM_LINUX
#include <cerrno>
#include <dirent.h>
#include <sys/stat.h>
#endif

// Shared only by isolated automation fixtures. A failed check must retain the fixture.
namespace SaveGameTestPathSafety
{
	enum class EFixturePathKind : uint8 { Missing, Directory, File, Unsafe };

	inline FString FixtureAbsolutePath(const FString& InPath)
	{
		FString Path = FPaths::ConvertRelativePathToFull(InPath);
		FPaths::NormalizeDirectoryName(Path);
		if (FPaths::IsRelative(Path) || !FPaths::CollapseRelativeDirectories(Path)) return FString();
#if PLATFORM_WINDOWS
		// This fixture deliberately refuses UNC and drive-relative paths.
		if (Path.Len() < 3 || !FChar::IsAlpha(Path[0]) || Path[1] != TEXT(':') || Path[2] != TEXT('/')) return FString();
#elif PLATFORM_LINUX
		if (!Path.StartsWith(TEXT("/"))) return FString();
#else
		return FString();
#endif
		return Path;
	}

	inline EFixturePathKind ProbeFixturePath(const FString& InPath)
	{
#if PLATFORM_WINDOWS
		FString Native = FPlatformFileManager::Get().GetPlatformFile().ConvertToAbsolutePathForExternalAppForWrite(*InPath);
		FPaths::MakePlatformFilename(Native);
		if (!Native.StartsWith(TEXT("\\\\?\\"))) Native = TEXT("\\\\?\\") + Native;
		const DWORD Attributes = ::GetFileAttributesW(*Native);
		if (Attributes == INVALID_FILE_ATTRIBUTES)
		{
			const DWORD Error = ::GetLastError();
			return Error == ERROR_FILE_NOT_FOUND || Error == ERROR_PATH_NOT_FOUND
				? EFixturePathKind::Missing : EFixturePathKind::Unsafe;
		}
		if (Attributes & FILE_ATTRIBUTE_REPARSE_POINT) return EFixturePathKind::Unsafe;
		return (Attributes & FILE_ATTRIBUTE_DIRECTORY) ? EFixturePathKind::Directory : EFixturePathKind::File;
#elif PLATFORM_LINUX
		const FTCHARToUTF8 Native(*InPath);
		struct stat Info;
		if (::lstat(Native.Get(), &Info) != 0)
			return errno == ENOENT ? EFixturePathKind::Missing : EFixturePathKind::Unsafe;
		if (S_ISDIR(Info.st_mode)) return EFixturePathKind::Directory;
		if (S_ISREG(Info.st_mode)) return EFixturePathKind::File;
		return EFixturePathKind::Unsafe;
#else
		return EFixturePathKind::Unsafe;
#endif
	}

	inline bool ValidateFixtureDirectoryChain(const FString& InPath, const bool bAllowMissing)
	{
		FString Path = FixtureAbsolutePath(InPath);
		if (Path.IsEmpty()) return false;
		for (int32 Depth = 0; Depth < 128; ++Depth)
		{
			const EFixturePathKind Kind = ProbeFixturePath(Path);
			if (Kind != EFixturePathKind::Directory && !(bAllowMissing && Kind == EFixturePathKind::Missing)) return false;
#if PLATFORM_WINDOWS
			if (Path.Len() == 3 && Path[1] == TEXT(':') && Path[2] == TEXT('/')) return Kind == EFixturePathKind::Directory;
#else
			if (Path == TEXT("/")) return Kind == EFixturePathKind::Directory;
#endif
			FString Parent = FPaths::GetPath(Path);
#if PLATFORM_WINDOWS
			if (Parent.Len() == 2 && Parent[1] == TEXT(':')) Parent += TEXT("/");
#else
			if (Parent.IsEmpty() && Path.StartsWith(TEXT("/"))) Parent = TEXT("/");
#endif
			if (Parent.IsEmpty() || FPaths::IsSamePath(Parent, Path)) return false;
			Path = MoveTemp(Parent);
		}
		return false;
	}

	inline bool CreateExclusiveFixtureRoot(const FString& InParent, const FString& InName, FString& OutRoot)
	{
		const FString Parent = FixtureAbsolutePath(InParent);
		const FString Saved = FixtureAbsolutePath(FPaths::ProjectSavedDir());
		const FString Project = FixtureAbsolutePath(FPaths::ProjectDir());
		if (Parent.IsEmpty() || Saved.IsEmpty() || Project.IsEmpty() || InName.IsEmpty() ||
			!FPaths::IsUnderDirectory(Saved, Project) || !FPaths::IsUnderDirectory(Parent, Saved) ||
			!ValidateFixtureDirectoryChain(Parent, true)) return false;
		OutRoot = FixtureAbsolutePath(FPaths::Combine(Parent, InName));
		if (OutRoot.IsEmpty() || !FPaths::IsSamePath(FPaths::GetPath(OutRoot), Parent) ||
			!FPaths::GetCleanFilename(OutRoot).Equals(InName, ESearchCase::CaseSensitive)) return false;
		if (!IFileManager::Get().MakeDirectory(*Parent, true) || !ValidateFixtureDirectoryChain(Parent, false) ||
			ProbeFixturePath(OutRoot) != EFixturePathKind::Missing) return false;
#if PLATFORM_WINDOWS
		FString Native = FPlatformFileManager::Get().GetPlatformFile().ConvertToAbsolutePathForExternalAppForWrite(*OutRoot);
		FPaths::MakePlatformFilename(Native);
		if (!Native.StartsWith(TEXT("\\\\?\\"))) Native = TEXT("\\\\?\\") + Native;
		if (!::CreateDirectoryW(*Native, nullptr)) return false;
#elif PLATFORM_LINUX
		const FTCHARToUTF8 Native(*OutRoot);
		if (::mkdir(Native.Get(), 0700) != 0) return false;
#else
		return false;
#endif
		return ValidateFixtureDirectoryChain(OutRoot, false);
	}


	inline bool VisitFixtureDirectory(const FString& InDirectory, TFunctionRef<bool(const FString&, bool)> InVisitor)
	{
		const FString Directory = FixtureAbsolutePath(InDirectory);
		if (Directory.IsEmpty() || ProbeFixturePath(Directory) != EFixturePathKind::Directory) return false;
		auto VisitName = [&](const FString& Name)
		{
			if (Name == TEXT(".") || Name == TEXT("..")) return true;
			const FString Child = FixtureAbsolutePath(FPaths::Combine(Directory, Name));
			if (Child.IsEmpty() || !FPaths::IsSamePath(FPaths::GetPath(Child), Directory)) return false;
			const EFixturePathKind Kind = ProbeFixturePath(Child);
			return (Kind == EFixturePathKind::Directory || Kind == EFixturePathKind::File) &&
				InVisitor(Child, Kind == EFixturePathKind::Directory);
		};
#if PLATFORM_WINDOWS
		FString Native = FPlatformFileManager::Get().GetPlatformFile().ConvertToAbsolutePathForExternalAppForWrite(*Directory);
		FPaths::MakePlatformFilename(Native);
		if (!Native.StartsWith(TEXT("\\\\?\\"))) Native = TEXT("\\\\?\\") + Native;
		const HANDLE DirectoryHandle = ::CreateFileW(*Native, FILE_LIST_DIRECTORY,
			FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
			FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
		if (DirectoryHandle == INVALID_HANDLE_VALUE) return false;
		BY_HANDLE_FILE_INFORMATION Info;
		if (!::GetFileInformationByHandle(DirectoryHandle, &Info) ||
			!(Info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ||
			(Info.dwFileAttributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM)))
		{
			::CloseHandle(DirectoryHandle);
			return false;
		}
		WIN32_FIND_DATAW Data;
		const FString Pattern = Native + (Native.EndsWith(TEXT("\\")) ? TEXT("*") : TEXT("\\*"));
		const HANDLE Search = ::FindFirstFileW(*Pattern, &Data);
		if (Search == INVALID_HANDLE_VALUE)
		{
			const DWORD Error = ::GetLastError();
			::CloseHandle(DirectoryHandle);
			return Error == ERROR_FILE_NOT_FOUND;
		}
		bool bValid = true;
		for (;;)
		{
			if (!VisitName(Data.cFileName)) { bValid = false; break; }
			if (!::FindNextFileW(Search, &Data))
			{
				bValid = ::GetLastError() == ERROR_NO_MORE_FILES;
				break;
			}
		}
		const bool bClosedSearch = ::FindClose(Search) != 0;
		const bool bClosedDirectory = ::CloseHandle(DirectoryHandle) != 0;
		return bValid && bClosedSearch && bClosedDirectory;
#elif PLATFORM_LINUX
		const FTCHARToUTF8 Native(*Directory);
		DIR* Handle = ::opendir(Native.Get());
		if (!Handle) return false;
		bool bValid = true;
		for (;;)
		{
			errno = 0;
			const dirent* Entry = ::readdir(Handle);
			if (!Entry) { bValid = errno == 0; break; }
			if (!VisitName(UTF8_TO_TCHAR(Entry->d_name))) { bValid = false; break; }
		}
		return ::closedir(Handle) == 0 && bValid;
#else
		return false;
#endif
	}

	inline bool ValidateFixtureTree(const FString& InRoot)
	{
		const FString Root = FixtureAbsolutePath(InRoot);
		if (Root.IsEmpty() || !ValidateFixtureDirectoryChain(Root, false)) return false;
		TArray<FString> Directories{Root};
		while (!Directories.IsEmpty())
		{
			const FString Directory = Directories.Pop(EAllowShrinking::No);
			if (ProbeFixturePath(Directory) != EFixturePathKind::Directory) return false;
			if (!VisitFixtureDirectory(Directory, [&](const FString& InChild, const bool bDirectory)
			{
				const FString Child = FixtureAbsolutePath(InChild);
				if (Child.IsEmpty() || !FPaths::IsUnderDirectory(Child, Root) ||
					!FPaths::IsSamePath(FPaths::GetPath(Child), Directory)) return false;
				const EFixturePathKind Kind = ProbeFixturePath(Child);
				if (bDirectory && Kind == EFixturePathKind::Directory) Directories.Add(Child);
				else if (bDirectory || Kind != EFixturePathKind::File) return false;
				return true;
			})) return false;
		}
		return true;
	}

	inline bool FixtureDirectoryIsEmpty(const FString& InDirectory)
	{
		if (!ValidateFixtureDirectoryChain(InDirectory, false)) return false;
		bool bEmpty = true;
		return VisitFixtureDirectory(InDirectory,
			[&bEmpty](const FString&, bool) { bEmpty = false; return true; }) && bEmpty;
	}
}

#endif
