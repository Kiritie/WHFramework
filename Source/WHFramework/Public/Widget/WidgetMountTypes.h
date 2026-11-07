#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "ObjectPool/ObjectPoolModuleTypes.h"

#include "WidgetMountTypes.generated.h"

class ULocalPlayer;
class UUserWidget;
class UUserWidgetBase;

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FSubWidgetSpawnParameter : public FWidgetSpawnParameter
{
	GENERATED_BODY()

public:
	FSubWidgetSpawnParameter() = default;

	FSubWidgetSpawnParameter(UObject* InOwnerObject, bool bInDynamic)
		: FWidgetSpawnParameter(InOwnerObject)
		, bDynamic(bInDynamic)
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bDynamic = false;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FCommonOptionSelectorSpawnParameter : public FSubWidgetSpawnParameter
{
	GENERATED_BODY()

public:
	FCommonOptionSelectorSpawnParameter() = default;

	FCommonOptionSelectorSpawnParameter(
		const TArray<FString>& InOptions,
		bool bInEditable)
		: Options(InOptions)
		, bEditable(bInEditable)
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FString> Options;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bEditable = false;
};

USTRUCT()
struct WHFRAMEWORK_API FSubWidgetRuntimeEntry
{
	GENERATED_BODY()

public:
	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> Widget = nullptr;

	UPROPERTY(Transient)
	bool bDynamic = false;
};

USTRUCT()
struct WHFRAMEWORK_API FScreenWidgetKey
{
    GENERATED_BODY()

    UPROPERTY(Transient)
    TObjectPtr<ULocalPlayer> LocalPlayer = nullptr;

    UPROPERTY(Transient)
    FGameplayTag WidgetTag;

    bool operator==(const FScreenWidgetKey& Other) const
    {
        return LocalPlayer == Other.LocalPlayer && WidgetTag == Other.WidgetTag;
    }
};

FORCEINLINE uint32 GetTypeHash(const FScreenWidgetKey& Value)
{
    return HashCombine(GetTypeHash(Value.LocalPlayer), GetTypeHash(Value.WidgetTag));
}

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FWidgetMountContext
{
	GENERATED_BODY()

public:
	UPROPERTY(Transient, BlueprintReadOnly)
	TObjectPtr<ULocalPlayer> LocalPlayer = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (Categories = "Widget.Screen"))
	FGameplayTag ParentWidgetTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (Categories = "Widget.Slot"))
	FGameplayTag SlotTag;

	bool operator==(const FWidgetMountContext& Other) const
	{
		return LocalPlayer == Other.LocalPlayer && ParentWidgetTag == Other.ParentWidgetTag && SlotTag == Other.SlotTag;
	}
};

FORCEINLINE uint32 GetTypeHash(const FWidgetMountContext& Value)
{
	return HashCombine(GetTypeHash(Value.LocalPlayer), HashCombine(GetTypeHash(Value.ParentWidgetTag), GetTypeHash(Value.SlotTag)));
}

USTRUCT()
struct WHFRAMEWORK_API FTemporaryWidgetHistory
{
	GENERATED_BODY()

public:
	UPROPERTY(Transient)
	TArray<TObjectPtr<UUserWidgetBase>> Widgets;
};
