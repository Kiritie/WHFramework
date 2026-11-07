#pragma once

#include "Setting/SettingDefinitionTypes.h"
#include "Setting/SettingModuleTypes.h"
#include "SettingEditTypes.generated.h"

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FSettingEditState
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly)
	bool bActive = false;

	UPROPERTY(BlueprintReadOnly)
	bool bDirty = false;

	UPROPERTY(BlueprintReadOnly)
	bool bCanApply = false;

	UPROPERTY(BlueprintReadOnly)
	bool bCanReset = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSettingEditStateChanged, const FSettingEditState&, InState);

USTRUCT()
struct WHFRAMEWORK_API FSettingEditSession
{
	GENERATED_BODY()

public:
	UPROPERTY(Transient)
	FSettingModuleSaveData AppliedData;

	UPROPERTY(Transient)
	FSettingModuleSaveData PendingData;

	UPROPERTY(Transient)
	FSettingModuleSaveData DefaultData;

	UPROPERTY(Transient)
	bool bActive = false;
};

USTRUCT()
struct WHFRAMEWORK_API FSettingConfirmationTransaction
{
	GENERATED_BODY()

public:
	UPROPERTY(Transient)
	FSettingModuleSaveData PreviousData;

	UPROPERTY(Transient)
	TArray<FSettingId> SettingIds;

	UPROPERTY(Transient)
	double ExpiresAt = 0.0;

	UPROPERTY(Transient)
	bool bActive = false;
};
