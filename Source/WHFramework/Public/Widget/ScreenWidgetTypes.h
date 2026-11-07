#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Slate/Runtime/Interfaces/ScreenWidgetInterface.h"
#include "Widgets/Layout/Anchors.h"

#include "ScreenWidgetTypes.generated.h"

class UUserWidgetBase;

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FWidgetOpenParameter
{
	GENERATED_BODY()

public:
	FWidgetOpenParameter() = default;

	explicit FWidgetOpenParameter(UObject* InOwnerObject)
		: OwnerObject(InOwnerObject)
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UObject> OwnerObject = nullptr;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FWidgetDelegateOpenParameter : public FWidgetOpenParameter
{
	GENERATED_BODY()

public:
	FWidgetDelegateOpenParameter() = default;

	explicit FWidgetDelegateOpenParameter(const FSimpleDynamicDelegate& InDelegate)
		: Delegate(InDelegate)
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FSimpleDynamicDelegate Delegate;
};

UENUM(BlueprintType)
enum class EWidgetInputMode : uint8
{
	None,
	GameOnly,
	UIOnly,
	GameAndUI
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FScreenWidgetConfig
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Widget")
	TSubclassOf<UUserWidgetBase> WidgetClass;

	UPROPERTY(EditAnywhere, Category = "Widget", meta = (Categories = "Widget.Screen"))
	FGameplayTag WidgetTagOverride;

	UPROPERTY(EditAnywhere, Category = "Widget")
	EWidgetCreateType CreateType = EWidgetCreateType::None;

	UPROPERTY(EditAnywhere, Category = "Widget")
	EWidgetType WidgetType = EWidgetType::Permanent;

	UPROPERTY(EditAnywhere, Category = "Topology", meta = (Categories = "Widget.Slot"))
	FGameplayTag SlotTag;

	UPROPERTY(EditAnywhere, Category = "Layout")
	int32 ZOrder = 0;

	UPROPERTY(EditAnywhere, Category = "Layout")
	FAnchors Anchors = FAnchors(0.f, 0.f, 1.f, 1.f);

	UPROPERTY(EditAnywhere, Category = "Layout")
	bool bAutoSize = false;

	UPROPERTY(EditAnywhere, Category = "Layout")
	FMargin Offsets = FMargin(0.f);

	UPROPERTY(EditAnywhere, Category = "Layout")
	FVector2D Alignment = FVector2D::ZeroVector;

	FGameplayTag ResolveWidgetTag() const;

	FGameplayTag ResolveParentWidgetTag() const;
};

/**
* 世界Widget可见类型
*/
