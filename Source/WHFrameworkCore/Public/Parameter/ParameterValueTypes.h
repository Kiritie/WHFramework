#pragma once

#include "GameplayTagContainer.h"
#include "InputCoreTypes.h"
#include "Math/IntPoint.h"
#include "Styling/SlateBrush.h"
#include "StructUtils/InstancedStruct.h"
#include "UObject/PrimaryAssetId.h"
#include "WHFrameworkCoreTypes.h"
#include "ParameterValueTypes.generated.h"

USTRUCT(BlueprintType)
struct WHFRAMEWORKCORE_API FParameterValueBase
{
	GENERATED_BODY()
};

USTRUCT(meta = (ParameterHidden))
struct WHFRAMEWORKCORE_API FParameterStructValue : public FParameterValueBase
{
	GENERATED_BODY()

	UPROPERTY()
	FInstancedStruct Value;
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

USTRUCT(BlueprintType, meta = (DisplayName = "Integer", ParameterInlineValue))
struct WHFRAMEWORKCORE_API FParameterIntValue : public FParameterValueBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Value = 0;
};
USTRUCT(BlueprintType, meta = (DisplayName = "Integer 64", ParameterInlineValue))
struct WHFRAMEWORKCORE_API FParameterInt64Value : public FParameterValueBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int64 Value = 0;
};
USTRUCT(BlueprintType, meta = (DisplayName = "Float", ParameterInlineValue))
struct WHFRAMEWORKCORE_API FParameterFloatValue : public FParameterValueBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Value = 0.f;
};
USTRUCT(BlueprintType, meta = (DisplayName = "Double", ParameterInlineValue))
struct WHFRAMEWORKCORE_API FParameterDoubleValue : public FParameterValueBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	double Value = 0.0;
};
USTRUCT(BlueprintType, meta = (DisplayName = "Byte", ParameterInlineValue))
struct WHFRAMEWORKCORE_API FParameterByteValue : public FParameterValueBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	uint8 Value = 0;
};
USTRUCT(BlueprintType, meta = (DisplayName = "Enum", ParameterInlineValue))
struct WHFRAMEWORKCORE_API FParameterEnumValue : public FParameterValueBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FEnumParameterValue Value;
};
USTRUCT(BlueprintType, meta = (DisplayName = "String", ParameterInlineValue))
struct WHFRAMEWORKCORE_API FParameterStringValue : public FParameterValueBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FString Value;
};
USTRUCT(BlueprintType, meta = (DisplayName = "Name", ParameterInlineValue))
struct WHFRAMEWORKCORE_API FParameterNameValue : public FParameterValueBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName Value = NAME_None;
};
USTRUCT(BlueprintType, meta = (DisplayName = "Text", ParameterInlineValue))
struct WHFRAMEWORKCORE_API FParameterTextValue : public FParameterValueBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText Value;
};
USTRUCT(BlueprintType, meta = (DisplayName = "Boolean", ParameterInlineValue))
struct WHFRAMEWORKCORE_API FParameterBoolValue : public FParameterValueBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool Value = false;
};
USTRUCT(BlueprintType, meta = (DisplayName = "Vector", ParameterInlineValue))
struct WHFRAMEWORKCORE_API FParameterVectorValue : public FParameterValueBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector Value = FVector::ZeroVector;
};
USTRUCT(BlueprintType, meta = (DisplayName = "Rotator", ParameterInlineValue))
struct WHFRAMEWORKCORE_API FParameterRotatorValue : public FParameterValueBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FRotator Value = FRotator::ZeroRotator;
};
USTRUCT(BlueprintType, meta = (DisplayName = "Transform", ParameterInlineValue))
struct WHFRAMEWORKCORE_API FParameterTransformValue : public FParameterValueBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FTransform Value = FTransform::Identity;
};
USTRUCT(BlueprintType, meta = (DisplayName = "Color", ParameterInlineValue))
struct WHFRAMEWORKCORE_API FParameterColorValue : public FParameterValueBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FColor Value = FColor::Transparent;
};
USTRUCT(BlueprintType, meta = (DisplayName = "Linear Color", ParameterInlineValue))
struct WHFRAMEWORKCORE_API FParameterLinearColorValue : public FParameterValueBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FLinearColor Value = FLinearColor::Transparent;
};
USTRUCT(BlueprintType, meta = (DisplayName = "Key", ParameterInlineValue))
struct WHFRAMEWORKCORE_API FParameterKeyValue : public FParameterValueBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FKey Value;
};
USTRUCT(BlueprintType, meta = (DisplayName = "Integer Point", ParameterInlineValue))
struct WHFRAMEWORKCORE_API FParameterIntPointValue : public FParameterValueBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FIntPoint Value = FIntPoint::ZeroValue;
};
USTRUCT(BlueprintType, meta = (DisplayName = "Gameplay Tag", ParameterInlineValue))
struct WHFRAMEWORKCORE_API FParameterTagValue : public FParameterValueBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGameplayTag Value;
};
USTRUCT(BlueprintType, meta = (DisplayName = "Gameplay Tags", ParameterInlineValue))
struct WHFRAMEWORKCORE_API FParameterTagsValue : public FParameterValueBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGameplayTagContainer Value;
};
USTRUCT(BlueprintType, meta = (DisplayName = "Slate Brush", ParameterInlineValue))
struct WHFRAMEWORKCORE_API FParameterBrushValue : public FParameterValueBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FSlateBrush Value;
};
USTRUCT(BlueprintType, meta = (DisplayName = "Guid", ParameterInlineValue))
struct WHFRAMEWORKCORE_API FParameterGuidValue : public FParameterValueBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGuid Value;
};
USTRUCT(BlueprintType, meta = (DisplayName = "Primary Asset Id", ParameterInlineValue))
struct WHFRAMEWORKCORE_API FParameterAssetIdValue : public FParameterValueBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FPrimaryAssetId Value;
};
USTRUCT(BlueprintType, meta = (DisplayName = "Class", ParameterInlineValue))
struct WHFRAMEWORKCORE_API FParameterClassValue : public FParameterValueBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UClass> Value = nullptr;
};
USTRUCT(BlueprintType, meta = (DisplayName = "Soft Class", ParameterInlineValue))
struct WHFRAMEWORKCORE_API FParameterSoftClassValue : public FParameterValueBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSoftClassPtr<UObject> Value;
};
USTRUCT(BlueprintType, meta = (DisplayName = "Object", ParameterInlineValue))
struct WHFRAMEWORKCORE_API FParameterObjectValue : public FParameterValueBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UObject> Value = nullptr;
};
USTRUCT(BlueprintType, meta = (DisplayName = "Soft Object", ParameterInlineValue))
struct WHFRAMEWORKCORE_API FParameterSoftObjectValue : public FParameterValueBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSoftObjectPtr<UObject> Value;
};

USTRUCT(BlueprintType, meta = (DisplayName = "Object Instance", ParameterInlineValue))
struct WHFRAMEWORKCORE_API FParameterObjectInstanceValue : public FParameterValueBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Instanced)
	TObjectPtr<UObject> Value = nullptr;
};

USTRUCT(BlueprintType, meta = (DisplayName = "Dynamic Delegate", ParameterInlineValue))
struct WHFRAMEWORKCORE_API FParameterDelegateValue : public FParameterValueBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FSimpleDynamicDelegate Value;
};
