#pragma once

#include "Ability/AbilityModuleTypes.h"
#include "SaveGame/SaveGameModuleTypes.h"
#include "Scene/SceneModuleTypes.h"

#include "SaveDataInterface.generated.h"

UINTERFACE()
class WHFRAMEWORK_API USaveDataInterface : public UInterface
{
	GENERATED_BODY()
};

class WHFRAMEWORK_API ISaveDataInterface
{
	GENERATED_BODY()

public:
	void LoadSaveData(FSaveData* InSaveData, bool bForceMode = false, bool bLoadMemoryData = false);

	FSaveData* ToSaveData(bool bSaveMemoryData = false);

	template<class T>
	T* ToSaveData(bool bSaveMemoryData = false)
	{
		return static_cast<T*>(ToSaveData(bSaveMemoryData));
	}

	template<class T>
	T& ToSaveDataRef(bool bSaveMemoryData = false)
	{
		return *ToSaveData<T>(bSaveMemoryData);
	}

	void UnloadSaveData(bool bForceMode = false, bool bUnLoadMemoryData = false);

protected:
	virtual void LoadData(FSaveData* InSaveData, bool bForceMode) = 0;

	virtual FSaveData* ToData() = 0;

	virtual void UnloadData(bool bForceMode) { }
};
