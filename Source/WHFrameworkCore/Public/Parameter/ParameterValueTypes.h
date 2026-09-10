#pragma once

#include "GameplayTagContainer.h"
#include "InputCoreTypes.h"
#include "Styling/SlateBrush.h"
#include "UObject/PrimaryAssetId.h"
#include "WHFrameworkCoreTypes.h"
#include "ParameterValueTypes.generated.h"

USTRUCT(BlueprintType)
struct WHFRAMEWORKCORE_API FParameterValueBase
{
	GENERATED_BODY()
};

USTRUCT(BlueprintType)
struct WHFRAMEWORKCORE_API FEnumParameterValue
{
	GENERATED_BODY()

	FEnumParameterValue();
	explicit FEnumParameterValue(UEnum* InEnumType, uint8 InEnumValue);
	explicit FEnumParameterValue(const FString& InEnumName, uint8 InEnumValue);
	explicit FEnumParameterValue(const TArray<FString>& InEnumNames, uint8 InEnumValue);

	friend bool operator==(const FEnumParameterValue& A, const FEnumParameterValue& B)
	{
		return A.EnumType == B.EnumType && A.EnumName == B.EnumName && A.EnumNames == B.EnumNames && A.EnumValue == B.EnumValue;
	}
	friend bool operator!=(const FEnumParameterValue& A, const FEnumParameterValue& B)
	{
		return !(A == B);
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UEnum> EnumType = nullptr;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString EnumName;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FString> EnumNames;
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	uint8 EnumValue = 0;
};

USTRUCT(BlueprintType, meta = (DisplayName = "Integer"))
struct WHFRAMEWORKCORE_API FParameterIntValue : public FParameterValueBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Value = 0;
};
USTRUCT(BlueprintType, meta = (DisplayName = "Float"))
struct WHFRAMEWORKCORE_API FParameterFloatValue : public FParameterValueBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Value = 0.f;
};
USTRUCT(BlueprintType, meta = (DisplayName = "Byte"))
struct WHFRAMEWORKCORE_API FParameterByteValue : public FParameterValueBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	uint8 Value = 0;
};
USTRUCT(BlueprintType, meta = (DisplayName = "Enum"))
struct WHFRAMEWORKCORE_API FParameterEnumValue : public FParameterValueBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FEnumParameterValue Value;
};
USTRUCT(BlueprintType, meta = (DisplayName = "String"))
struct WHFRAMEWORKCORE_API FParameterStringValue : public FParameterValueBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Value;
};
USTRUCT(BlueprintType, meta = (DisplayName = "Name"))
struct WHFRAMEWORKCORE_API FParameterNameValue : public FParameterValueBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName Value = NAME_None;
};
USTRUCT(BlueprintType, meta = (DisplayName = "Text"))
struct WHFRAMEWORKCORE_API FParameterTextValue : public FParameterValueBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText Value;
};
USTRUCT(BlueprintType, meta = (DisplayName = "Boolean"))
struct WHFRAMEWORKCORE_API FParameterBoolValue : public FParameterValueBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool Value = false;
};
USTRUCT(BlueprintType, meta = (DisplayName = "Vector"))
struct WHFRAMEWORKCORE_API FParameterVectorValue : public FParameterValueBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector Value = FVector::ZeroVector;
};
USTRUCT(BlueprintType, meta = (DisplayName = "Rotator"))
struct WHFRAMEWORKCORE_API FParameterRotatorValue : public FParameterValueBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FRotator Value = FRotator::ZeroRotator;
};
USTRUCT(BlueprintType, meta = (DisplayName = "Transform"))
struct WHFRAMEWORKCORE_API FParameterTransformValue : public FParameterValueBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FTransform Value = FTransform::Identity;
};
USTRUCT(BlueprintType, meta = (DisplayName = "Color"))
struct WHFRAMEWORKCORE_API FParameterColorValue : public FParameterValueBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FColor Value = FColor::Transparent;
};
USTRUCT(BlueprintType, meta = (DisplayName = "Linear Color"))
struct WHFRAMEWORKCORE_API FParameterLinearColorValue : public FParameterValueBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FLinearColor Value = FLinearColor::Transparent;
};
USTRUCT(BlueprintType, meta = (DisplayName = "Key"))
struct WHFRAMEWORKCORE_API FParameterKeyValue : public FParameterValueBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FKey Value;
};
USTRUCT(BlueprintType, meta = (DisplayName = "Gameplay Tag"))
struct WHFRAMEWORKCORE_API FParameterTagValue : public FParameterValueBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGameplayTag Value;
};
USTRUCT(BlueprintType, meta = (DisplayName = "Gameplay Tags"))
struct WHFRAMEWORKCORE_API FParameterTagsValue : public FParameterValueBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGameplayTagContainer Value;
};
USTRUCT(BlueprintType, meta = (DisplayName = "Slate Brush"))
struct WHFRAMEWORKCORE_API FParameterBrushValue : public FParameterValueBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FSlateBrush Value;
};
USTRUCT(BlueprintType, meta = (DisplayName = "Guid"))
struct WHFRAMEWORKCORE_API FParameterGuidValue : public FParameterValueBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGuid Value;
};
USTRUCT(BlueprintType, meta = (DisplayName = "Primary Asset Id"))
struct WHFRAMEWORKCORE_API FParameterAssetIdValue : public FParameterValueBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FPrimaryAssetId Value;
};
USTRUCT(BlueprintType, meta = (DisplayName = "Class"))
struct WHFRAMEWORKCORE_API FParameterClassValue : public FParameterValueBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UClass> Value = nullptr;
};
USTRUCT(BlueprintType, meta = (DisplayName = "Soft Class"))
struct WHFRAMEWORKCORE_API FParameterSoftClassValue : public FParameterValueBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSoftClassPtr<UObject> Value;
};
USTRUCT(BlueprintType, meta = (DisplayName = "Object"))
struct WHFRAMEWORKCORE_API FParameterObjectValue : public FParameterValueBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UObject> Value = nullptr;
};
USTRUCT(BlueprintType, meta = (DisplayName = "Soft Object"))
struct WHFRAMEWORKCORE_API FParameterSoftObjectValue : public FParameterValueBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSoftObjectPtr<UObject> Value;
};

USTRUCT(BlueprintType, meta = (DisplayName = "Object Instance"))
struct WHFRAMEWORKCORE_API FParameterObjectInstanceValue : public FParameterValueBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Instanced)
	TObjectPtr<UObject> Value = nullptr;
};

USTRUCT(BlueprintType, meta = (DisplayName = "Dynamic Delegate"))
struct WHFRAMEWORKCORE_API FParameterDelegateValue : public FParameterValueBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FSimpleDynamicDelegate Value;
};

/** Runtime-only compatibility payload. It is hidden from Details and intentionally has no reflected value. */
USTRUCT(meta = (Hidden))
struct WHFRAMEWORKCORE_API FParameterPointerValue : public FParameterValueBase
{
	GENERATED_BODY()
	void* Value = nullptr;
};
