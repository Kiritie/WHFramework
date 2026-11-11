#pragma once

#include "Common/LifecycleTypes.h"
#include "Common/CommonTypes.h"
#include "Parameter/ParameterTypes.h"
#include "SaveGame/SaveGameModuleTypes.h"

#include "SaveDataAgentInterface.generated.h"

UINTERFACE()
class WHFRAMEWORK_API USaveDataAgentInterface : public UInterface
{
	GENERATED_BODY()
};

class WHFRAMEWORK_API ISaveDataAgentInterface
{
	GENERATED_BODY()

public:
	bool LoadSaveData(const FParameter& InSaveData, EPhase InPhase = EPhase::All);

	FParameter GetSaveData(bool bRefresh = false);

	bool UnloadSaveData(EPhase InPhase = EPhase::All);
	virtual bool WasLoadDataApplied() const { return bLoadDataApplied; }

protected:
	virtual void LoadData(const FParameter& InSaveData, EPhase InPhase) = 0;

	virtual FParameter ToData() = 0;

	virtual void UnloadData(EPhase InPhase) { }

	virtual void ResetData() { }

	virtual FParameter GetData() { return FParameter(); }

	virtual bool HasArchive() const { return false; }

	void MarkLoadDataFailed() { bLoadDataApplied = false; }
	void MarkUnloadDataFailed() { bUnloadDataApplied = false; }

private:
	bool bLoadDataApplied = true;
	bool bUnloadDataApplied = true;
	static const FSaveData* ResolveSaveData(const FParameter& InParameter);

	static FSaveData* ResolveSaveData(FParameter& InParameter);
};
