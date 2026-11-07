#pragma once

#include "CoreMinimal.h"
#include "Parameter/ParameterTypes.h"
#include "Setting/SettingPresentationTypes.h"
#include "SettingDefinitionTypes.generated.h"

class UEnum;
class UWidgetCustomSettingValueBase;

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
	FText CategoryDisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 CategoryOrder = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Order = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ESettingRendererType Renderer = ESettingRendererType::Auto;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<UWidgetCustomSettingValueBase> CustomWidgetClass;

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
	bool bOverrideCategoryOrder = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "bOverrideCategoryOrder"))
	int32 CategoryOrder = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bOverrideOrder = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "bOverrideOrder"))
	int32 Order = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bOverrideRenderer = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "bOverrideRenderer"))
	ESettingRendererType Renderer = ESettingRendererType::Auto;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bOverrideCustomWidgetClass = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (EditCondition = "bOverrideCustomWidgetClass"))
	TSubclassOf<UWidgetCustomSettingValueBase> CustomWidgetClass;

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
struct WHFRAMEWORK_API FSettingConfig
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FSettingPageDefinition> Pages;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FSettingDefinitionOverride> Overrides;
};
