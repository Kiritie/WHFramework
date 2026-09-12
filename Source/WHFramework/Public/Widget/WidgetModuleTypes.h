// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "SaveGame/SaveGameModuleTypes.h"
#include "Slate/Runtime/Interfaces/ScreenWidgetInterface.h"
#include "Widgets/Layout/Anchors.h"

#include "WidgetModuleTypes.generated.h"

class UUserWidgetBase;
class UWorldWidgetBase;

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FWidgetMountContext
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (Categories = "Widget"))
	FGameplayTag ParentWidgetTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (Categories = "WidgetSlot"))
	FGameplayTag ParentSlotTag;

	bool operator==(const FWidgetMountContext& Other) const
	{
		return ParentWidgetTag == Other.ParentWidgetTag && ParentSlotTag == Other.ParentSlotTag;
	}
};

FORCEINLINE uint32 GetTypeHash(const FWidgetMountContext& Value)
{
	return HashCombine(GetTypeHash(Value.ParentWidgetTag), GetTypeHash(Value.ParentSlotTag));
}

UENUM(BlueprintType)
enum class EWidgetInputConfig : uint8
{
	None,
	Game,
	GameAndMenu,
	Menu
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FScreenWidgetConfig
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Widget")
	TSubclassOf<UUserWidgetBase> WidgetClass;

	UPROPERTY(EditAnywhere, Category = "Widget", meta = (Categories = "Widget"))
	FGameplayTag WidgetTagOverride;

	UPROPERTY(EditAnywhere, Category = "Widget")
	EWidgetCreateType CreateType = EWidgetCreateType::None;

	UPROPERTY(EditAnywhere, Category = "Widget")
	EWidgetType WidgetType = EWidgetType::Permanent;

	UPROPERTY(EditAnywhere, Category = "Topology", meta = (Categories = "Widget"))
	FGameplayTag ParentWidgetTag;

	UPROPERTY(EditAnywhere, Category = "Topology", meta = (Categories = "WidgetSlot"))
	FGameplayTag ParentSlotTag;

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
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FWorldWidgetConfig
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "Widget")
	TSubclassOf<UWorldWidgetBase> WidgetClass;

	UPROPERTY(EditAnywhere, Category = "Widget", meta = (Categories = "WidgetWorld"))
	FGameplayTag WidgetTagOverride;

	FGameplayTag ResolveWidgetTag() const;
};

/**
* 世界Widget可见类型
*/
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

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FWidgetModuleSaveData : public FSaveData
{
	GENERATED_BODY()

public:
	FORCEINLINE FWidgetModuleSaveData()
	{
		LanguageType = 0;
		GlobalScale = 1.f;
	}

public:
	UPROPERTY()
	int32 LanguageType;

	UPROPERTY()
	float GlobalScale;
};
