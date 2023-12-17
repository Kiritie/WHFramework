// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "WidgetModuleTypes.generated.h"

/**
* Widget类型
*/
UENUM(BlueprintType)
enum class EWidgetType : uint8
{
	/// 常驻
	Permanent,
	/// 临时
	Temporary
};

/**
* Widget创建类型
*/
UENUM(BlueprintType)
enum class EWidgetCreateType : uint8
{
	/// 无
	None,
	/// 自动创建
	AutoCreate,
	/// 自动创建并打开
	AutoCreateAndOpen
};

/**
* Widget打开类型
*/
UENUM(BlueprintType)
enum class EWidgetOpenType : uint8
{
	/// 无
	None,
	/// 显示
	Visible,
	/// 显示并禁用点击
	HitTestInvisible,
	/// 显示并禁用自身点击
	SelfHitTestInvisible
};

/**
* Widget打开完成类型
*/
UENUM(BlueprintType)
enum class EWidgetOpenFinishType : uint8
{
	/// 瞬间
	Instant,
	/// 延时
	Delay,
	/// 动画
	Animator,
	/// 程序调用
	Procedure
};

/**
* Widget关闭类型
*/
UENUM(BlueprintType)
enum class EWidgetCloseType : uint8
{
	/// 无
	None,
	/// 隐藏
	Hidden,
	/// 塌陷
	Collapsed,
	/// 移除
	Remove
};

/**
* Widget关闭完成类型
*/
UENUM(BlueprintType)
enum class EWidgetCloseFinishType : uint8
{
	/// 瞬间
	Instant,
	/// 延时
	Delay,
	/// 动画
	Animator,
	/// 程序调用
	Procedure
};

/**
* Widget刷新类型
*/
UENUM(BlueprintType)
enum class EWidgetRefreshType : uint8
{
	/// 无
	None,
	/// 帧更新
	Tick,
	/// 计时器
	Timer,
	/// 程序
	Procedure
};

/**
* 屏幕Widget状态
*/
UENUM(BlueprintType)
enum class EScreenWidgetState : uint8
{
	/// 无
	None,
	/// 打开中
	Opening,
	/// 已打开
	Opened,
	/// 关闭中
	Closing,
	/// 已关闭
	Closed
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FWorldWidgets
{
	GENERATED_USTRUCT_BODY()

public:
	FWorldWidgets()
	{
		WorldWidgets = TArray<class UWorldWidgetBase*>();
	}
	
public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	TArray<class UWorldWidgetBase*> WorldWidgets;
};

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FWorldWidgetBindInfo
{
	GENERATED_USTRUCT_BODY()

public:
	FWorldWidgetBindInfo()
	{
		Location = FVector();
		SceneComp = nullptr;
		SocketName = NAME_None;
	}
	
	FWorldWidgetBindInfo(FVector InLocation) : FWorldWidgetBindInfo()
	{
		Location = InLocation;
	}
		
	FWorldWidgetBindInfo(USceneComponent* InSceneComp, const FName InSocketName = NAME_None) : FWorldWidgetBindInfo()
	{
		SceneComp = InSceneComp;
		SocketName = InSocketName;
	}

public:
	UPROPERTY(BlueprintReadWrite)
	FVector Location;

	UPROPERTY(BlueprintReadWrite)
	USceneComponent* SceneComp;

	UPROPERTY(BlueprintReadWrite)
	FName SocketName;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWidgetStateChanged, EScreenWidgetState, InWidgetState);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FK2_OnWidgetOpened, const TArray<FParameter>&, InParams, bool, bInstant);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FK2_OnWidgetClosed, bool, bInstant);

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnWidgetOpened, const TArray<FParameter>&, bool);

DECLARE_MULTICAST_DELEGATE_OneParam(FOnWidgetClosed, bool);
