#pragma once

#include "Common/CommonModuleTypes.h"
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
	void LoadSaveData(const FParameter& InSaveData, EPhase InPhase = EPhase::All);

	FParameter GetSaveData(bool bRefresh = false);

	void UnloadSaveData(EPhase InPhase = EPhase::All);

protected:
	virtual void LoadData(const FParameter& InSaveData, EPhase InPhase) = 0;

	virtual FParameter ToData() = 0;

	virtual void UnloadData(EPhase InPhase) { }

	virtual void ResetData() { }

	virtual FParameter GetData() { return FParameter(); }

	virtual bool HasArchive() const { return false; }

private:
	static const FSaveData* ResolveSaveData(const FParameter& InParameter);

	static FSaveData* ResolveSaveData(FParameter& InParameter);
};
