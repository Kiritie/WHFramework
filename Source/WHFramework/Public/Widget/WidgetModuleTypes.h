// Fill out your copyright notice in the Description page of Project Settings.

#pragma once
#include "SaveGame/SaveGameModuleTypes.h"

#include "WidgetModuleTypes.generated.h"

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
