#pragma once

#include "CoreMinimal.h"

// 本地运行实例的存档命名空间与跨进程所有权，不随 World 切换。
class WHFRAMEWORK_API FSaveGameLocalContext
{
public:
	FSaveGameLocalContext();
	~FSaveGameLocalContext();
	FSaveGameLocalContext(const FSaveGameLocalContext&) = delete;
	FSaveGameLocalContext& operator=(const FSaveGameLocalContext&) = delete;

	bool Acquire(const FString& InSaveGamesRoot, const FString& InRequestedNamespace, FString& OutError);
	const FString& GetNamespace() const { return Namespace; }
	const FString& GetStorageRoot() const { return StorageRoot; }
	const FString& GetSlotPrefix() const { return SlotPrefix; }
	const FString& GetUnboundInventoryRoot() const { return UnboundInventoryRoot; }

private:
	int64 LeaseHandle = -1;
	FString Namespace;
	FString StorageRoot;
	FString SlotPrefix;
	FString UnboundInventoryRoot;
};
