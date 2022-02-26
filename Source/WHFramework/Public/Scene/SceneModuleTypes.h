// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "SceneModuleTypes.generated.h"

DECLARE_DYNAMIC_DELEGATE(FOnAsyncLoadLevelFinished);
DECLARE_DYNAMIC_DELEGATE(FOnAsyncUnloadLevelFinished);

UENUM(BlueprintType)
enum class EProcedureType1 : uint8
{
	Root,
	Default,
	Standalone
};
