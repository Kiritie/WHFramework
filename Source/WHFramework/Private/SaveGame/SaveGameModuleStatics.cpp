#include "SaveGame/SaveGameModuleStatics.h"
#include "SaveGame/SaveOperationTypes.h"
#include "SaveGame/SaveSlotTypes.h"
#include "Common/LifecycleTypes.h"

#include "SaveGame/SaveGameModule.h"

int32 USaveGameModuleStatics::GetUserIndex(const UObject* WorldContextObject)
{
	USaveGameModule* Saves = USaveGameModule::GetPtr(WorldContextObject ? WorldContextObject->GetWorld() : nullptr);
	return Saves ? Saves->GetUserIndex() : INDEX_NONE;
}

void USaveGameModuleStatics::SetUserIndex(const UObject* WorldContextObject, int32 InUserIndex)
{
	USaveGameModule* Saves = USaveGameModule::GetPtr(WorldContextObject ? WorldContextObject->GetWorld() : nullptr);
	if (Saves) Saves->SetUserIndex(InUserIndex);
	else UE_LOG(LogTemp, Warning, TEXT("保存操作未执行：原世界的 SaveGameModule 不可用。"));
}

FSaveOperationResult USaveGameModuleStatics::CreateSaveSlot(const UObject* WorldContextObject, const FCreateSaveSlotParams& Params, FSaveSlotSummary& OutSummary)
{
	USaveGameModule* Saves = USaveGameModule::GetPtr(WorldContextObject ? WorldContextObject->GetWorld() : nullptr);
	if (!Saves) OutSummary = FSaveSlotSummary();
	return Saves ? Saves->CreateSaveSlot(Params, OutSummary) : FSaveOperationResult::Failed(ESaveResultCode::ModuleMissing, NSLOCTEXT("WH.Save", "MissingWorldModule", "原世界的保存模块不可用。"));
}

FSaveOperationResult USaveGameModuleStatics::SaveActiveSlot(const UObject* WorldContextObject)
{
	USaveGameModule* Saves = USaveGameModule::GetPtr(WorldContextObject ? WorldContextObject->GetWorld() : nullptr);
	return Saves ? Saves->SaveActiveSlot() : FSaveOperationResult::Failed(ESaveResultCode::ModuleMissing, NSLOCTEXT("WH.Save", "MissingWorldModule", "原世界的保存模块不可用。"));
}

FGuid USaveGameModuleStatics::BeginPendingSaveSlot(const UObject* WorldContextObject, const FCreateSaveSlotParams& Params)
{
	USaveGameModule* Saves = USaveGameModule::GetPtr(WorldContextObject ? WorldContextObject->GetWorld() : nullptr);
	return Saves ? Saves->BeginPendingSaveSlot(Params) : FGuid();
}

bool USaveGameModuleStatics::CancelPendingSaveSlot(const UObject* WorldContextObject, const FGuid& InToken)
{
	USaveGameModule* Saves = USaveGameModule::GetPtr(WorldContextObject ? WorldContextObject->GetWorld() : nullptr);
	return Saves && Saves->CancelPendingSaveSlot(InToken);
}

bool USaveGameModuleStatics::IsPendingSaveSlot(const UObject* WorldContextObject, const FGuid& InToken)
{
	const USaveGameModule* Saves = USaveGameModule::GetPtr(WorldContextObject ? WorldContextObject->GetWorld() : nullptr);
	return Saves && Saves->IsPendingSaveSlot(InToken);
}

FSaveOperationResult USaveGameModuleStatics::SavePendingSlot(const UObject* WorldContextObject, const FGuid& InToken)
{
	USaveGameModule* Saves = USaveGameModule::GetPtr(WorldContextObject ? WorldContextObject->GetWorld() : nullptr);
	return Saves ? Saves->SavePendingSlot(InToken) : FSaveOperationResult::Failed(ESaveResultCode::ModuleMissing, NSLOCTEXT("WH.Save", "MissingWorldModule", "原世界的保存模块不可用。"));
}

bool USaveGameModuleStatics::ConsumePendingSlotResult(const UObject* WorldContextObject, const FGuid& InToken, FSaveOperationResult& OutResult)
{
	USaveGameModule* Saves = USaveGameModule::GetPtr(WorldContextObject ? WorldContextObject->GetWorld() : nullptr);
	return Saves && Saves->ConsumePendingSlotResult(InToken, OutResult);
}

bool USaveGameModuleStatics::HasPendingSaveSlot(const UObject* WorldContextObject)
{
	USaveGameModule* Saves = USaveGameModule::GetPtr(WorldContextObject ? WorldContextObject->GetWorld() : nullptr);
	return Saves ? Saves->HasPendingSaveSlot() : false;
}

FSaveOperationResult USaveGameModuleStatics::SaveCurrentSlot(const UObject* WorldContextObject)
{
	USaveGameModule* Saves = USaveGameModule::GetPtr(WorldContextObject ? WorldContextObject->GetWorld() : nullptr);
	return Saves ? Saves->SaveCurrentSlot() : FSaveOperationResult::Failed(ESaveResultCode::ModuleMissing, NSLOCTEXT("WH.Save", "MissingWorldModule", "原世界的保存模块不可用。"));
}

FSaveOperationResult USaveGameModuleStatics::GetWorldSaveResult(const UObject* WorldContextObject, const FGuid& InOperationId)
{
	USaveGameModule* Saves = USaveGameModule::GetPtr(WorldContextObject ? WorldContextObject->GetWorld() : nullptr);
	return Saves ? Saves->GetWorldSaveResult(InOperationId) : FSaveOperationResult::Failed(ESaveResultCode::ModuleMissing, NSLOCTEXT("WH.Save", "MissingWorldModule", "原世界的保存模块不可用。"));
}

void USaveGameModuleStatics::DetachActiveSave(const UObject* WorldContextObject)
{
	USaveGameModule* Saves = USaveGameModule::GetPtr(WorldContextObject ? WorldContextObject->GetWorld() : nullptr);
	if (Saves) Saves->DetachActiveSave();
	else UE_LOG(LogTemp, Warning, TEXT("保存操作未执行：原世界的 SaveGameModule 不可用。"));
}

void USaveGameModuleStatics::ClearActiveSave(const UObject* WorldContextObject)
{
	USaveGameModule* Saves = USaveGameModule::GetPtr(WorldContextObject ? WorldContextObject->GetWorld() : nullptr);
	if (Saves) Saves->ClearActiveSave();
	else UE_LOG(LogTemp, Warning, TEXT("保存操作未执行：原世界的 SaveGameModule 不可用。"));
}

FSaveOperationResult USaveGameModuleStatics::SaveSlot(const UObject* WorldContextObject, FGuid SaveId)
{
	USaveGameModule* Saves = USaveGameModule::GetPtr(WorldContextObject ? WorldContextObject->GetWorld() : nullptr);
	return Saves ? Saves->SaveSlot(SaveId) : FSaveOperationResult::Failed(ESaveResultCode::ModuleMissing, NSLOCTEXT("WH.Save", "MissingWorldModule", "原世界的保存模块不可用。"));
}

FSaveOperationResult USaveGameModuleStatics::LoadSaveSlot(const UObject* WorldContextObject, FGuid SaveId, EPhase InPhase)
{
	USaveGameModule* Saves = USaveGameModule::GetPtr(WorldContextObject ? WorldContextObject->GetWorld() : nullptr);
	return Saves ? Saves->LoadSlot(SaveId, InPhase) : FSaveOperationResult::Failed(ESaveResultCode::ModuleMissing, NSLOCTEXT("WH.Save", "MissingWorldModule", "原世界的保存模块不可用。"));
}

FSaveOperationResult USaveGameModuleStatics::DeleteSaveSlot(const UObject* WorldContextObject, FGuid SaveId)
{
	USaveGameModule* Saves = USaveGameModule::GetPtr(WorldContextObject ? WorldContextObject->GetWorld() : nullptr);
	return Saves ? Saves->DeleteSaveSlot(SaveId) : FSaveOperationResult::Failed(ESaveResultCode::ModuleMissing, NSLOCTEXT("WH.Save", "MissingWorldModule", "原世界的保存模块不可用。"));
}

FSaveOperationResult USaveGameModuleStatics::RenameSaveSlot(const UObject* WorldContextObject, FGuid SaveId, const FString& NewName)
{
	USaveGameModule* Saves = USaveGameModule::GetPtr(WorldContextObject ? WorldContextObject->GetWorld() : nullptr);
	return Saves ? Saves->RenameSaveSlot(SaveId, NewName) : FSaveOperationResult::Failed(ESaveResultCode::ModuleMissing, NSLOCTEXT("WH.Save", "MissingWorldModule", "原世界的保存模块不可用。"));
}

FSaveOperationResult USaveGameModuleStatics::DuplicateSaveSlot(const UObject* WorldContextObject, FGuid SourceSaveId, const FString& NewName, FSaveSlotSummary& OutSummary)
{
	USaveGameModule* Saves = USaveGameModule::GetPtr(WorldContextObject ? WorldContextObject->GetWorld() : nullptr);
	if (!Saves) OutSummary = FSaveSlotSummary();
	return Saves ? Saves->DuplicateSaveSlot(SourceSaveId, NewName, OutSummary) : FSaveOperationResult::Failed(ESaveResultCode::ModuleMissing, NSLOCTEXT("WH.Save", "MissingWorldModule", "原世界的保存模块不可用。"));
}

FSaveSlotSummary USaveGameModuleStatics::GetSaveSlotSummary(const UObject* WorldContextObject, FGuid SaveId)
{
	USaveGameModule* Saves = USaveGameModule::GetPtr(WorldContextObject ? WorldContextObject->GetWorld() : nullptr);
	return Saves ? Saves->GetSaveSlotSummary(SaveId) : FSaveSlotSummary();
}

TArray<FSaveSlotSummary> USaveGameModuleStatics::GetSaveSlotSummaries(const UObject* WorldContextObject)
{
	USaveGameModule* Saves = USaveGameModule::GetPtr(WorldContextObject ? WorldContextObject->GetWorld() : nullptr);
	return Saves ? Saves->GetSaveSlotSummaries() : TArray<FSaveSlotSummary>();
}

bool USaveGameModuleStatics::HasActiveSave(const UObject* WorldContextObject)
{
	USaveGameModule* Saves = USaveGameModule::GetPtr(WorldContextObject ? WorldContextObject->GetWorld() : nullptr);
	return Saves ? Saves->HasActiveSave() : false;
}

FGuid USaveGameModuleStatics::GetActiveSaveId(const UObject* WorldContextObject)
{
	USaveGameModule* Saves = USaveGameModule::GetPtr(WorldContextObject ? WorldContextObject->GetWorld() : nullptr);
	return Saves ? Saves->GetActiveSaveId() : FGuid();
}

FSaveOperationResult USaveGameModuleStatics::SaveProfile(const UObject* WorldContextObject, FName ProfileName)
{
	USaveGameModule* Saves = USaveGameModule::GetPtr(WorldContextObject ? WorldContextObject->GetWorld() : nullptr);
	return Saves ? Saves->SaveProfile(ProfileName) : FSaveOperationResult::Failed(ESaveResultCode::ModuleMissing, NSLOCTEXT("WH.Save", "MissingWorldModule", "原世界的保存模块不可用。"));
}

FSaveOperationResult USaveGameModuleStatics::LoadProfile(const UObject* WorldContextObject, FName ProfileName)
{
	USaveGameModule* Saves = USaveGameModule::GetPtr(WorldContextObject ? WorldContextObject->GetWorld() : nullptr);
	return Saves ? Saves->LoadProfile(ProfileName) : FSaveOperationResult::Failed(ESaveResultCode::ModuleMissing, NSLOCTEXT("WH.Save", "MissingWorldModule", "原世界的保存模块不可用。"));
}
