#pragma once

#include "Ability/AbilityModuleTypes.h"
#include "Global/GlobalTypes.h"
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
	void LoadSaveData(FSaveData* InSaveData, EPhase InPhase = EPhase::Lesser);

	FSaveData* GetSaveData(bool bRefresh = false);

	template<class T>
	T* GetSaveData(bool bRefresh = false)
	{
		return static_cast<T*>(GetSaveData(bRefresh));
	}

	template<class T>
	T& GetSaveDataRef(bool bRefresh = false)
	{
		return *GetSaveData<T>(bRefresh);
	}

	void UnloadSaveData(EPhase InPhase = EPhase::Lesser);

protected:
	virtual void LoadData(FSaveData* InSaveData, EPhase InPhase) = 0;

	virtual FSaveData* ToData() = 0;

	template<class T>
	T* LocalData(const T& InSaveData)
	{
		if(!LocalSaveData) LocalSaveData = new T();
		*static_cast<T*>(LocalSaveData) = InSaveData;
		return static_cast<T*>(LocalSaveData);
	}

	virtual void UnloadData(EPhase InPhase) { }

	virtual bool HasArchive() const { return false; }

private:
	FSaveData* LocalSaveData = nullptr;
};
