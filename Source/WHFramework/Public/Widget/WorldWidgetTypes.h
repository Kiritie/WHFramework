#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Widgets/Layout/Anchors.h"

#include "WorldWidgetTypes.generated.h"

class UWorldWidgetBase;
class USceneComponent;

UENUM(BlueprintType)
enum class EWorldWidgetSpace : uint8
{
	World,
	Screen
};

UENUM(BlueprintType)
enum class EWorldWidgetVisibility : uint8
{
	/// 无
	None,
	/// 始终显示
	AlwaysShow = 1 << 0,
	/// 仅渲染
	RenderOnly = 1 << 1,
	/// 仅屏幕
	ScreenOnly = 1 << 2,
	/// 仅距离
	DistanceOnly = 1 << 3,
	/// 渲染和距离
	RenderAndDistance = RenderOnly | DistanceOnly,
	/// 屏幕和距离
	ScreenAndDistance = ScreenOnly | DistanceOnly,
	/// 屏幕渲染和距离
	RenderScreenAndDistance = RenderOnly | ScreenOnly | DistanceOnly
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FWorldWidgetConfig
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Widget")
	TSubclassOf<UWorldWidgetBase> WidgetClass;

	UPROPERTY(EditAnywhere, Category = "Widget", meta = (Categories = "Widget.World"))
	FGameplayTag WidgetTagOverride;

	UPROPERTY(EditAnywhere, Category = "Layout")
	EWorldWidgetSpace Space = EWorldWidgetSpace::Screen;

	UPROPERTY(EditAnywhere, Category = "Layout", meta = (EditConditionHides, EditCondition = "Space == EWorldWidgetSpace::Screen"))
	int32 ZOrder = 0;

	UPROPERTY(EditAnywhere, Category = "Layout", meta = (EditConditionHides, EditCondition = "Space == EWorldWidgetSpace::Screen"))
	FAnchors Anchors = FAnchors(0.f, 0.f, 0.f, 0.f);

	UPROPERTY(EditAnywhere, Category = "Layout")
	bool bAutoSize = false;

	UPROPERTY(EditAnywhere, Category = "Layout", meta = (EditConditionHides, EditCondition = "Space == EWorldWidgetSpace::World && bAutoSize == false"))
	FVector2D DrawSize = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, Category = "Layout", meta = (EditConditionHides, EditCondition = "Space == EWorldWidgetSpace::Screen && bAutoSize == false"))
	FMargin Offsets = FMargin(0.f);

	UPROPERTY(EditAnywhere, Category = "Layout")
	FVector2D Alignment = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, Category = "Visibility")
	EWorldWidgetVisibility Visibility = EWorldWidgetVisibility::AlwaysShow;

	UPROPERTY(EditAnywhere, Category = "Visibility", meta = (EditConditionHides, EditCondition = "Visibility == EWorldWidgetVisibility::DistanceOnly || Visibility == EWorldWidgetVisibility::RenderAndDistance || Visibility == EWorldWidgetVisibility::ScreenAndDistance || Visibility == EWorldWidgetVisibility::RenderScreenAndDistance"))
	float ShowDistance = -1.f;

	FGameplayTag ResolveWidgetTag() const;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FWorldWidgets
{
	GENERATED_USTRUCT_BODY()

public:
	FWorldWidgets()
	{
		WorldWidgets = TArray<class UWorldWidgetBase*>();
		bVisible = true;
	}

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	TArray<class UWorldWidgetBase*> WorldWidgets;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite)
	bool bVisible;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FWorldWidgetMapping
{
	GENERATED_USTRUCT_BODY()

public:
	FWorldWidgetMapping()
	{
		SceneComp = nullptr;
		SocketName = NAME_None;
		Location = FVector();
	}

	FWorldWidgetMapping(const FVector& InLocation) : FWorldWidgetMapping()
	{
		Location = InLocation;
	}

	FWorldWidgetMapping(USceneComponent* InSceneComp, const FName InSocketName = NAME_None, const FVector& InLocation = FVector::ZeroVector) : FWorldWidgetMapping()
	{
		SceneComp = InSceneComp;
		SocketName = InSocketName;
		Location = InLocation;
	}

public:
	FVector GetLocation() const;

public:
	UPROPERTY(BlueprintReadWrite)
	USceneComponent* SceneComp;

	UPROPERTY(BlueprintReadWrite, meta = (EditConditionHides, EditCondition = "SceneComp != nullptr"))
	FName SocketName;

	UPROPERTY(BlueprintReadWrite)
	FVector Location;
};
