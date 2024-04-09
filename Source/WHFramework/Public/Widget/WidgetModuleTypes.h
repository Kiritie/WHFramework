// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


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
	AlwaysShow,
	/// 仅渲染
	RenderOnly,
	/// 仅屏幕
	ScreenOnly,
	/// 仅距离
	DistanceOnly,
	/// 渲染和距离
	RenderAndDistance,
	/// 屏幕和距离
	ScreenAndDistance
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
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TArray<class UWorldWidgetBase*> WorldWidgets;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
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
	UPROPERTY(BlueprintReadWrite)
	USceneComponent* SceneComp;

	UPROPERTY(BlueprintReadWrite, meta = (EditConditionHides, EditCondition = "SceneComp != nullptr"))
	FName SocketName;

	UPROPERTY(BlueprintReadWrite)
	FVector Location;
};
