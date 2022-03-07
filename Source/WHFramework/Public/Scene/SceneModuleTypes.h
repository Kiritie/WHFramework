// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "SceneModuleTypes.generated.h"

DECLARE_DYNAMIC_DELEGATE_OneParam(FOnAsyncLoadLevelFinished, FName, InLevelPath);
DECLARE_DYNAMIC_DELEGATE_OneParam(FOnAsyncUnloadLevelFinished, FName, InLevelPath);

USTRUCT()
struct FSceneModuleStruct
{
	GENERATED_BODY()
};
