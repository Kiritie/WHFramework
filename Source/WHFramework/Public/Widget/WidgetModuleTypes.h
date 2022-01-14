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
	/// 半常驻
	SemiPermanent,
	/// 临时
	Temporary,
	/// 半临时
	SemiTemporary
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
