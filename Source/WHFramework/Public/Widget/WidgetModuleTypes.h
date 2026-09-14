// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "ObjectPool/ObjectPoolModuleTypes.h"
#include "SaveGame/SaveGameModuleTypes.h"
#include "Slate/Runtime/Interfaces/ScreenWidgetInterface.h"
#include "Widgets/Layout/Anchors.h"

#include "WidgetModuleTypes.generated.h"

class UUserWidgetBase;
class UUserWidget;
class UWorldWidgetBase;

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
		const FText& InTitle,
		const TArray<FString>& InOptions,
		bool bInEditable)
		: Title(InTitle)
		, Options(InOptions)
		, bEditable(bInEditable)
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText Title;

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

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FWidgetMountContext
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (Categories = "Widget.Screen"))
	FGameplayTag ParentWidgetTag;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (Categories = "Widget.Slot"))
	FGameplayTag SlotTag;

	bool operator==(const FWidgetMountContext& Other) const
	{
		return ParentWidgetTag == Other.ParentWidgetTag && SlotTag == Other.SlotTag;
	}
};

FORCEINLINE uint32 GetTypeHash(const FWidgetMountContext& Value)
{
	return HashCombine(GetTypeHash(Value.ParentWidgetTag), GetTypeHash(Value.SlotTag));
}

UENUM(BlueprintType)
enum class EWidgetInputMode : uint8
{
	None,
	GameOnly,
	UIOnly,
	GameAndUI
};

UENUM(BlueprintType)
enum class EWorldWidgetSpace : uint8
{
	World,
	Screen
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
	UPROPERTY(meta = (SettingRenderer = "Option", SettingApply = "Immediate", SettingCategory = "Global", SettingOrder = "0"))
	int32 LanguageType;

	UPROPERTY(meta = (ClampMin = "0.5", ClampMax = "2.0", SettingApply = "Preview", SettingCategory = "Global", SettingOrder = "10"))
	float GlobalScale;
};
