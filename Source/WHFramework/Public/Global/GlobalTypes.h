// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"

#include "GlobalTypes.generated.h"

extern WHFRAMEWORK_API bool GIsPlaying;
extern WHFRAMEWORK_API bool GIsSimulating;

extern UObject* GetWorldContext(bool bInEditor = false);

UENUM(BlueprintType)
enum class EPauseMode : uint8
{
	Default,
	OnlyTime,
	OnlyModules
};

UENUM(BlueprintType)
enum class EPhase : uint8
{
	Primary,
	Lesser,
	Final
};

/*
* 数据保存档案
*/
struct FSaveDataArchive : public FObjectAndNameAsStringProxyArchive
{
	FSaveDataArchive(FArchive& InInnerArchive) : FObjectAndNameAsStringProxyArchive(InInnerArchive, false)
	{
		ArIsSaveGame = true;
	}
};

#define DON(Index, Count, Expression) \
for(int32 Index = 0; Index < Count; Index++) \
{ \
	Expression \
}
