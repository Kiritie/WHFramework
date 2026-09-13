// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "Audio/AudioModuleTypes.h"
#include "Camera/CameraModuleTypes.h"
#include "Input/InputModuleTypes.h"
#include "ObjectPool/ObjectPoolModuleTypes.h"
#include "SaveGame/SaveGameModuleTypes.h"
#include "Video/VideoModuleTypes.h"
#include "Widget/WidgetModuleTypes.h"

#include "SettingModuleTypes.generated.h"

class UEnum;

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FSettingId
{
	GENERATED_BODY()

public:
	FSettingId() = default;

	explicit FSettingId(FName InName)
		: Name(InName)
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName Name;

	bool IsValid() const
	{
		return !Name.IsNone();
	}

	bool operator==(const FSettingId& Other) const
	{
		return Name == Other.Name;
	}
};

FORCEINLINE uint32 GetTypeHash(const FSettingId& Value)
{
	return GetTypeHash(Value.Name);
}

UENUM(BlueprintType)
enum class ESettingRendererType : uint8
{
	Auto,
	Bool,
	Number,
	Enum,
	Text,
	Option,
	Key,
	Custom
};

UENUM(BlueprintType)
enum class ESettingApplyPolicy : uint8
{
	Deferred,
	Preview,
	Immediate
};

UENUM(BlueprintType)
enum class ESettingConditionOp : uint8
{
	Equals,
	NotEquals,
	Greater,
	Less
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FSettingOption
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FParameter Value;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText DisplayName;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FSettingCondition
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FSettingId OtherSetting;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ESettingConditionOp Op = ESettingConditionOp::Equals;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FParameter Value;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FSettingValidationResult
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bValid = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText Error;

	static FSettingValidationResult Valid()
	{
		return FSettingValidationResult();
	}

	static FSettingValidationResult Invalid(const FText& InError)
	{
		FSettingValidationResult Result;
		Result.bValid = false;
		Result.Error = InError;
		return Result;
	}
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSettingValueChanged, FSettingId, InSettingId);

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FSettingNumberDisplay
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bHasMin = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	double Min = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bHasMax = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	double Max = 1.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	double Step = 0.01;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	double Scale = 1.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 DecimalPlaces = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText Suffix;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FSettingDefinition
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FSettingId SettingId;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FString SourcePath;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FName Provider;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName Page;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName Category;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Order = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ESettingRendererType Renderer = ESettingRendererType::Auto;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ESettingApplyPolicy ApplyPolicy = ESettingApplyPolicy::Deferred;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bVisible = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bRequiresConfirmation = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGameplayTag SemanticTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FSettingNumberDisplay NumberDisplay;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TObjectPtr<UEnum> Enum;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FSettingOption> Options;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FSettingCondition> VisibleConditions;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FSettingCondition> EnableConditions;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FSettingDefinitionOverride
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FSettingId SettingId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bOverrideDisplayName = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "bOverrideDisplayName"))
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bOverrideDescription = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "bOverrideDescription"))
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bOverridePage = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "bOverridePage"))
	FName Page;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bOverrideCategory = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "bOverrideCategory"))
	FName Category;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bOverrideOrder = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "bOverrideOrder"))
	int32 Order = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bOverrideRenderer = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "bOverrideRenderer"))
	ESettingRendererType Renderer = ESettingRendererType::Auto;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bOverrideApplyPolicy = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "bOverrideApplyPolicy"))
	ESettingApplyPolicy ApplyPolicy = ESettingApplyPolicy::Deferred;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bOverrideVisible = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "bOverrideVisible"))
	bool bVisible = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bOverrideEnabled = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "bOverrideEnabled"))
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bOverrideRequiresConfirmation = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "bOverrideRequiresConfirmation"))
	bool bRequiresConfirmation = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bOverrideNumberDisplay = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "bOverrideNumberDisplay"))
	FSettingNumberDisplay NumberDisplay;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bOverrideVisibleConditions = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "bOverrideVisibleConditions"))
	TArray<FSettingCondition> VisibleConditions;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bOverrideEnableConditions = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "bOverrideEnableConditions"))
	TArray<FSettingCondition> EnableConditions;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FSettingPageDefinition
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName Page;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Order = 0;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FWidgetPressAnyKeyOpenParameter : public FWidgetOpenParameter
{
	GENERATED_BODY()

public:
	FWidgetPressAnyKeyOpenParameter() = default;

	explicit FWidgetPressAnyKeyOpenParameter(int32 InKeyIndex)
		: KeyIndex(InKeyIndex)
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 KeyIndex = INDEX_NONE;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FSettingModuleSaveData : public FSaveData
{
	GENERATED_BODY()

public:
	FORCEINLINE FSettingModuleSaveData()
	{
		WidgetData = FWidgetModuleSaveData();
		AudioData = FAudioModuleSaveData();
		VideoData = FVideoModuleSaveData();
		CameraData = FCameraModuleSaveData();
		InputData = FInputModuleSaveData();
		ParameterData = FParameterModuleSaveData();
	}

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FWidgetModuleSaveData WidgetData;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FAudioModuleSaveData AudioData;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVideoModuleSaveData VideoData;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FCameraModuleSaveData CameraData;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FInputModuleSaveData InputData;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FParameterModuleSaveData ParameterData;

public:
	virtual void MakeSaved() override
	{
		Super::MakeSaved();
		
		WidgetData.MakeSaved();
		AudioData.MakeSaved();
		VideoData.MakeSaved();
		CameraData.MakeSaved();
		InputData.MakeSaved();
		ParameterData.MakeSaved();
	}
};

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

USTRUCT()
struct FStringArrayParameterValue : public FParameterValueBase
{
	GENERATED_BODY()

	FStringArrayParameterValue() = default;
	explicit FStringArrayParameterValue(const TArray<FString>& InValue) : Value(InValue) { }

	UPROPERTY()
	TArray<FString> Value;
};

USTRUCT()
struct FInt32ArrayParameterValue : public FParameterValueBase
{
	GENERATED_BODY()

	FInt32ArrayParameterValue() = default;
	explicit FInt32ArrayParameterValue(const TArray<int32>& InValue) : Value(InValue) { }

	UPROPERTY()
	TArray<int32> Value;
};
