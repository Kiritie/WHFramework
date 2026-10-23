#include "SaveGame/SaveGameLocalContext.h"

#include "Ability/Inventory/InventoryStoreLock.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"

FSaveGameLocalContext::FSaveGameLocalContext()
{
}

FSaveGameLocalContext::~FSaveGameLocalContext()
{
	FInventoryStoreLock::Release(LeaseHandle);
}

bool FSaveGameLocalContext::Acquire(const FString& InSaveGamesRoot, const FString& InRequestedNamespace, FString& OutError)
{
	OutError.Reset();
	if (LeaseHandle != -1 || InSaveGamesRoot.IsEmpty())
	{
		OutError = TEXT("本地存档实例已取得所有权，或根目录无效。");
		return false;
	}
	bool bValidNamespace = InRequestedNamespace.Len() <= 32;
	for (const TCHAR Character : InRequestedNamespace)
	{
		if (!(Character >= TEXT('a') && Character <= TEXT('z')) && !(Character >= TEXT('A') && Character <= TEXT('Z')) &&
			!(Character >= TEXT('0') && Character <= TEXT('9')) && Character != TEXT('_') && Character != TEXT('-'))
		{
			bValidNamespace = false;
			break;
		}
	}
	if (!bValidNamespace)
	{
		OutError = TEXT("LocalDataId须为1–32位字母、数字、下划线或连字符。");
		return false;
	}
	const bool bAutomatic = InRequestedNamespace.IsEmpty();
	FString Root = FPaths::ConvertRelativePathToFull(InSaveGamesRoot);
	FPaths::NormalizeDirectoryName(Root);
	for (int32 Index = 0; Index < (bAutomatic ? 64 : 1); ++Index)
	{
		const FString Candidate = bAutomatic ? (Index == 0 ? TEXT("default") : FString::FromInt(Index)) : InRequestedNamespace;
		const bool bDefault = Candidate.Equals(TEXT("default"), ESearchCase::IgnoreCase);
		const FString Prefix = bDefault ? FString() : FPaths::Combine(TEXT("Instances"), Candidate);
		const FString Directory = Prefix.IsEmpty() ? Root : FPaths::Combine(Root, Prefix);
		const FString LockDirectory = FPaths::Combine(Directory, TEXT("InstanceOwnership"));
		if (!IFileManager::Get().MakeDirectory(*LockDirectory, true))
		{
			OutError = TEXT("无法创建本地存档实例目录：") + LockDirectory;
			return false;
		}
		FString LockError;
		const int64 Handle = FInventoryStoreLock::Acquire(LockDirectory, LockError);
		if (Handle != -1)
		{
			LeaseHandle = Handle;
			Namespace = bDefault ? TEXT("default") : Candidate;
			StorageRoot = Directory;
			SlotPrefix = Prefix;
			UnboundInventoryRoot = bDefault ? FPaths::Combine(FPaths::GetPath(Root), TEXT("Inventory"), TEXT("Unbound")) :
				FPaths::Combine(Directory, TEXT("Inventory"), TEXT("Unbound"));
			return true;
		}
		if (!bAutomatic || LockError != TEXT("Inventory.StoreAlreadyOpenOrLockUnavailable"))
		{
			OutError = TEXT("本地存档实例无法取得独占所有权：") + Candidate + TEXT("；") + LockError;
			return false;
		}
	}
	OutError = TEXT("本地存档实例目录均被占用，请关闭不再使用的实例或显式指定LocalDataId。");
	return false;
}
