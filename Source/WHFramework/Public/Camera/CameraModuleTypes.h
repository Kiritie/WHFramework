#pragma once

#include "Camera/CameraConfigTypes.h"
#include "SaveGame/SaveGameModuleTypes.h"
#include "CameraModuleTypes.generated.h"

USTRUCT(BlueprintType)
struct WHFRAMEWORK_API FCameraModuleSaveData : public FSaveData
{
	GENERATED_BODY()
	UPROPERTY()
	TMap<FString, FCameraUserSettings> Profiles;
};
