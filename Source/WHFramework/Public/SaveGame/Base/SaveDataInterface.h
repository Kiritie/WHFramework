#pragma once

#include "Common/CommonTypes.h"
#include "SaveGame/SaveGameModuleTypes.h"

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
	void LoadSaveData(FSaveData* InSaveData, EPhase InPhase = EPhase::All);

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

	void UnloadSaveData(EPhase InPhase = EPhase::All);

protected:
	virtual void LoadData(FSaveData* InSaveData, EPhase InPhase) = 0;

	virtual FSaveData* GetData() { return nullptr; }

	virtual FSaveData* ToData() = 0;

	virtual void UnloadData(EPhase InPhase) { }

	virtual bool HasArchive() const { return false; }
};
