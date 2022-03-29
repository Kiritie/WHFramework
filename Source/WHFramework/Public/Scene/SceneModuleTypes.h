// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "SceneModuleTypes.generated.h"

DECLARE_DYNAMIC_DELEGATE_OneParam(FOnAsyncLoadLevelFinished, FName, InLevelPath);
DECLARE_DYNAMIC_DELEGATE_OneParam(FOnAsyncUnloadLevelFinished, FName, InLevelPath);

/**
 * 世界文本类型
 */
UENUM(BlueprintType)
enum class EWorldTextType : uint8
{
	// 物理伤害
	PhysicsDamage,
	// 魔法伤害
	MagicDamage,
	// 格挡伤害
	DefendDamage,
	// 生命回复
	HealthRecover
};

/**
* 世界文本风格
*/
UENUM(BlueprintType)
enum class EWorldTextStyle : uint8
{
	// 普通
	Normal,
	// 强调
	Stress
};
