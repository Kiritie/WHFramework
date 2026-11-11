// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "MainModuleTypes.generated.h"

/**
* 模块状态
*/
UENUM(BlueprintType)
enum class EModuleState : uint8
{
	/// 无
	None,
	/// 运行中
	Running,
	/// 停止运行中
	UnRunning,
	/// 已暂停
	Paused,
	/// 已销毁
	Terminated
};
