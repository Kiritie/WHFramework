// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "MediaModuleTypes.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMoviePlayStartingDelegate, const FName&, InMoiveName);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMoviePlayFinishedDelegate, const FName&, InMoiveName);
DECLARE_DYNAMIC_DELEGATE_OneParam(FOnMoviePlayFinishedSingleDelegate, const FName&, InMoiveName);

//删了编译不过
/*
USTRUCT(BlueprintType)
struct FTestStruct
{
	GENERATED_USTRUCT_BODY()
};
*/
