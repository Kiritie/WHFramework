// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

/**
* 输入模式
*/
UENUM(BlueprintType)
enum class EInputMode : uint8
{
	/// 无
	None,
	/// UI模式
	UIOnly,
	/// 游戏和UI模式
	GameAndUI,
	/// 游戏模式
	GameOnly,
};

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
	/// 半常驻
	SemiPermanent,
	/// 临时
	Temporary,
	/// 半临时
	SemiTemporary
};
