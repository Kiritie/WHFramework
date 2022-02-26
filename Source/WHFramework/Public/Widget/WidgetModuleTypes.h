// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

/**
* Widget类型
*/
UENUM(BlueprintType)
enum class EWidgetType : uint8
{
	/// 无
	None,
	/// 常驻
	Permanent,
	/// 临时
	Temporary,
	/// 子界面
	Child
};

/**
* Widget打开类型
*/
UENUM(BlueprintType)
enum class EWidgetOpenType : uint8
{
	/// 显示
	Visible,
	/// 显示并禁用点击
	HitTestInvisible,
	/// 显示并禁用自身点击
	SelfHitTestInvisible
};

/**
* Widget关闭类型
*/
UENUM(BlueprintType)
enum class EWidgetCloseType : uint8
{
	/// 隐藏
	Hidden,
	/// 塌陷
	Collapsed,
	/// 移除
	Remove
};

/**
* Widget状态
*/
UENUM(BlueprintType)
enum class EWidgetState : uint8
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
