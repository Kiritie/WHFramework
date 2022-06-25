#pragma once

#include "Ability/AbilityModuleTypes.h"
#include "SaveGame/SaveGameModuleTypes.h"
#include "Scene/SceneModuleTypes.h"

#include "SaveDataInterface.generated.h"

class UAbilityBase;
UINTERFACE()
class WHFRAMEWORK_API USaveDataInterface : public UInterface
{
	GENERATED_BODY()
};

class WHFRAMEWORK_API ISaveDataInterface
{
	GENERATED_BODY()

public:
	virtual void LoadData(FSaveData* InSaveData) = 0;

	virtual FSaveData* ToData() = 0;

	virtual void UnloadData() { }
};
