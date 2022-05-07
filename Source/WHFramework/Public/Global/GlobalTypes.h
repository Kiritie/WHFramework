// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"

extern WHFRAMEWORK_API bool GIsPlaying;
extern WHFRAMEWORK_API bool GIsSimulating;

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
