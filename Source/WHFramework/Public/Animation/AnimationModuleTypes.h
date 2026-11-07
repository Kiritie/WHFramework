// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "CoreMinimal.h"
#include "AnimationModuleTypes.generated.h"

//////////////////////////////////////////////////////////////////////////
/// CancelableDelay
UENUM(BlueprintType)
namespace ECancelableDelayAction
{
	enum Type
	{	
		None,
		Cancel
	};
}
