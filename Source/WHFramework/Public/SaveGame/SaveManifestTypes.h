#pragma once

#include "CoreMinimal.h"
#include "SaveGame/SaveSlotTypes.h"

#include "SaveManifestTypes.generated.h"

USTRUCT()
struct WHFRAMEWORK_API FSaveManifest
{
	GENERATED_BODY()

	// 普通 Manifest 是整个世界分支唯一的发布回执。
	UPROPERTY()
	FGuid LastCommitOperationId;

	UPROPERTY()
	FGuid SaveId;

	UPROPERTY()
	FString DisplayName;

	UPROPERTY()
	FString Description;

	UPROPERTY()
	FDateTime CreatedAt;

	UPROPERTY()
	FDateTime UpdatedAt;

	UPROPERTY()
	FName CurrentMap;

	UPROPERTY()
	double PlayTimeSeconds = 0.0;

	UPROPERTY()
	FString PreviewFile = TEXT("preview.png");

	UPROPERTY()
	int32 StorageVersion = 1;

	UPROPERTY()
	int32 GameVersion = 1;

	UPROPERTY()
	int32 CurrentGeneration = 0;

	UPROPERTY()
	TMap<FName, int32> ModuleVersions;

	FSaveSlotSummary ToSummary(const FString& WorldDir) const;
};
