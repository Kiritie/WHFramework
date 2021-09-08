// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

/**
* 输入模式
*/
UENUM(BlueprintType)
enum class EWHInputMode : uint8
{
	/// 无
	None,
	/// UI模式
	UIOnly,
	/// 游戏模式
	GameOnly,
	/// 游戏和UI模式
	GameAndUI
};

/**
* Widget类型
*/
UENUM(BlueprintType)
enum class EWHWidgetType : uint8
{
	/// 无
	None,
	/// 临时
	Temporary,
	/// 常驻
	Permanent
};
