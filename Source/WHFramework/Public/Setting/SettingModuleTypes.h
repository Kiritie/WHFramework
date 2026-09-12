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

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FSettingId
{
	GENERATED_BODY()

public:
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

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FSettingNumberDisplay
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	double Min = 0.0;

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
	bool bRequiresConfirmation = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGameplayTag SemanticTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FSettingNumberDisplay NumberDisplay;
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
struct WHFRAMEWORK_API FWidgetSettingItemSpawnParameter : public FWidgetSpawnParameter
{
	GENERATED_BODY()

public:
	FWidgetSettingItemSpawnParameter() = default;

	explicit FWidgetSettingItemSpawnParameter(const FText& InTitle)
		: Title(InTitle)
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText Title;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FWidgetFloatSettingItemSpawnParameter : public FWidgetSettingItemSpawnParameter
{
	GENERATED_BODY()

public:
	FWidgetFloatSettingItemSpawnParameter() = default;

	FWidgetFloatSettingItemSpawnParameter(
		const FText& InTitle,
		float InMinValue,
		float InMaxValue,
		int32 InDecimalNum,
		float InScaleFactor = 1.f)
		: FWidgetSettingItemSpawnParameter(InTitle)
		, MinValue(InMinValue)
		, MaxValue(InMaxValue)
		, DecimalNum(InDecimalNum)
		, ScaleFactor(InScaleFactor)
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MinValue = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MaxValue = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 DecimalNum = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float ScaleFactor = 1.f;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FWidgetEnumSettingItemSpawnParameter : public FWidgetSettingItemSpawnParameter
{
	GENERATED_BODY()

public:
	FWidgetEnumSettingItemSpawnParameter() = default;

	FWidgetEnumSettingItemSpawnParameter(
		const FText& InTitle,
		const FString& InEnumName,
		const TArray<int32>& InIgnoredIndices = {})
		: FWidgetSettingItemSpawnParameter(InTitle)
		, EnumName(InEnumName)
		, IgnoredIndices(InIgnoredIndices)
	{
	}

	FWidgetEnumSettingItemSpawnParameter(
		const FText& InTitle,
		const TArray<FString>& InEnumNames,
		const TArray<int32>& InIgnoredIndices = {})
		: FWidgetSettingItemSpawnParameter(InTitle)
		, EnumNames(InEnumNames)
		, IgnoredIndices(InIgnoredIndices)
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString EnumName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FString> EnumNames;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<int32> IgnoredIndices;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FWidgetOptionSettingItemSpawnParameter : public FWidgetSettingItemSpawnParameter
{
	GENERATED_BODY()

public:
	FWidgetOptionSettingItemSpawnParameter() = default;

	FWidgetOptionSettingItemSpawnParameter(
		const FText& InTitle,
		const TArray<FString>& InOptions,
		bool bInEditable)
		: FWidgetSettingItemSpawnParameter(InTitle)
		, Options(InOptions)
		, bEditable(bInEditable)
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FString> Options;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bEditable = false;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FWidgetKeySettingItemSpawnParameter : public FWidgetSettingItemSpawnParameter
{
	GENERATED_BODY()

public:
	FWidgetKeySettingItemSpawnParameter() = default;

	FWidgetKeySettingItemSpawnParameter(
		const FText& InTitle,
		int32 InKeyNum,
		bool bInEditable)
		: FWidgetSettingItemSpawnParameter(InTitle)
		, KeyNum(InKeyNum)
		, bEditable(bInEditable)
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 KeyNum = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bEditable = false;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FWidgetSettingItemCategorySpawnParameter : public FWidgetSpawnParameter
{
	GENERATED_BODY()

public:
	FWidgetSettingItemCategorySpawnParameter() = default;

	explicit FWidgetSettingItemCategorySpawnParameter(const FText& InCategory)
		: Category(InCategory)
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText Category;
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
