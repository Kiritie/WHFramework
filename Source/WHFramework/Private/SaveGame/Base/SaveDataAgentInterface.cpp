#include "SaveGame/Base/SaveDataAgentInterface.h"
#include "WHFrameworkCoreMacros.h"
#include "Common/LifecycleTypes.h"

#include "Common/CommonModuleStatics.h"

const FSaveData* ISaveDataAgentInterface::ResolveSaveData(const FParameter& InParameter)
{
	const UScriptStruct* Struct = InParameter.GetStructType();
	const uint8* Memory = InParameter.GetStructMemory();
	return Struct && Memory && Struct->IsChildOf(FSaveData::StaticStruct()) ? reinterpret_cast<const FSaveData*>(Memory) : nullptr;
}

FSaveData* ISaveDataAgentInterface::ResolveSaveData(FParameter& InParameter)

{
	const UScriptStruct* Struct = InParameter.GetStructType();
	uint8* Memory = InParameter.GetMutableStructMemory();
	return Struct && Memory && Struct->IsChildOf(FSaveData::StaticStruct()) ? reinterpret_cast<FSaveData*>(Memory) : nullptr;
}

bool ISaveDataAgentInterface::LoadSaveData(const FParameter& InSaveData, EPhase InPhase)
{
    UObject* Owner = Cast<UObject>(this);
    if (!Owner || !InSaveData.HasValue()) return false;
    bLoadDataApplied = true;
    FParameter SaveData = InSaveData;
    if (PHASEC(InPhase, EPhase::Final) && HasArchive())
    {
        const FSaveData* Data = ResolveSaveData(SaveData);
        if (!Data) return false;
        if (!Data->GetDatas().IsEmpty())
        {
            TArray<uint8> Previous;
            if (!UCommonModuleStatics::SaveObjectDataToMemory(Owner, Previous)) return false;
            if (!UCommonModuleStatics::LoadObjectDataFromMemory(Owner, Data->GetDatas()))
            {
                if (!UCommonModuleStatics::LoadObjectDataFromMemory(Owner, Previous))
                    UE_LOG(LogTemp, Error, TEXT("Failed to restore the original object after archive read failed: %s"), *GetNameSafe(Owner));
                return false;
            }
        }
        else if (Data->IsSaved()) return false;
    }
    LoadData(SaveData, InPhase);
    return bLoadDataApplied && WasLoadDataApplied();
}

FParameter ISaveDataAgentInterface::GetSaveData(bool bRefresh)
{
    UObject* Owner = Cast<UObject>(this);
    if (!Owner) return FParameter();
    FParameter SaveData = bRefresh ? ToData() : GetData();
    if (!bRefresh && !SaveData.HasValue()) SaveData = ToData();
    if (!SaveData.HasValue()) return FParameter();
    if (HasArchive())
    {
        FSaveData* Data = ResolveSaveData(SaveData);
        if (!Data || !UCommonModuleStatics::SaveObjectDataToMemory(Owner, Data->GetDatas())) return FParameter();
    }
    if (FSaveData* Data = ResolveSaveData(SaveData)) Data->MakeSaved();
    return SaveData;
}

bool ISaveDataAgentInterface::UnloadSaveData(EPhase InPhase)
{
    UObject* Owner = Cast<UObject>(this);
    if (!Owner) return false;
    bUnloadDataApplied = true;
    UnloadData(InPhase);
    if (!bUnloadDataApplied) return false;
    if (PHASEC(InPhase, EPhase::Final) && HasArchive())
    {
        UObject* Archetype = Owner->GetArchetype();
        TArray<uint8> DefaultData;
        if (!Archetype || !UCommonModuleStatics::SaveObjectDataToMemory(Archetype, DefaultData))
        {
            UE_LOG(LogTemp, Error, TEXT("无法读取存档对象的默认状态：%s"), *GetNameSafe(Owner));
            return false;
        }
        if (!UCommonModuleStatics::LoadObjectDataFromMemory(Owner, DefaultData))
        {
            UE_LOG(LogTemp, Error, TEXT("无法恢复存档对象的默认状态：%s"), *GetNameSafe(Owner));
            return false;
        }
    }
    return true;
}