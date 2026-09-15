#include "SaveGame/SaveGameModuleStatics.h"

#include "SaveGame/SaveGameModule.h"

int32 USaveGameModuleStatics::GetUserIndex()
{
	return USaveGameModule::Get().GetUserIndex();
}

void USaveGameModuleStatics::SetUserIndex(int32 InUserIndex)
{
	USaveGameModule::Get().SetUserIndex(InUserIndex);
}

FSaveOperationResult USaveGameModuleStatics::CreateSaveSlot(const FCreateSaveSlotParams& Params, FSaveSlotSummary& OutSummary)
{
	return USaveGameModule::Get().CreateSaveSlot(Params, OutSummary);
}

FSaveOperationResult USaveGameModuleStatics::SaveActiveSlot()
{
	return USaveGameModule::Get().SaveActiveSlot();
}

void USaveGameModuleStatics::BeginPendingSaveSlot(const FCreateSaveSlotParams& Params)
{
	USaveGameModule::Get().BeginPendingSaveSlot(Params);
}

void USaveGameModuleStatics::CancelPendingSaveSlot()
{
	USaveGameModule::Get().CancelPendingSaveSlot();
}

bool USaveGameModuleStatics::HasPendingSaveSlot()
{
	return USaveGameModule::Get().HasPendingSaveSlot();
}

FSaveOperationResult USaveGameModuleStatics::SaveCurrentSlot()
{
	return USaveGameModule::Get().SaveCurrentSlot();
}

void USaveGameModuleStatics::ClearActiveSave()
{
	USaveGameModule::Get().ClearActiveSave();
}

FSaveOperationResult USaveGameModuleStatics::SaveSlot(FGuid SaveId)
{
	return USaveGameModule::Get().SaveSlot(SaveId);
}

FSaveOperationResult USaveGameModuleStatics::LoadSaveSlot(FGuid SaveId, EPhase InPhase)
{
	return USaveGameModule::Get().LoadSlot(SaveId, InPhase);
}

FSaveOperationResult USaveGameModuleStatics::DeleteSaveSlot(FGuid SaveId)
{
	return USaveGameModule::Get().DeleteSaveSlot(SaveId);
}

FSaveOperationResult USaveGameModuleStatics::RenameSaveSlot(FGuid SaveId, const FString& NewName)
{
	return USaveGameModule::Get().RenameSaveSlot(SaveId, NewName);
}

FSaveOperationResult USaveGameModuleStatics::DuplicateSaveSlot(FGuid SourceSaveId, const FString& NewName, FSaveSlotSummary& OutSummary)
{
	return USaveGameModule::Get().DuplicateSaveSlot(SourceSaveId, NewName, OutSummary);
}

FSaveSlotSummary USaveGameModuleStatics::GetSaveSlotSummary(FGuid SaveId)
{
	return USaveGameModule::Get().GetSaveSlotSummary(SaveId);
}

TArray<FSaveSlotSummary> USaveGameModuleStatics::GetSaveSlotSummaries()
{
	return USaveGameModule::Get().GetSaveSlotSummaries();
}

bool USaveGameModuleStatics::HasActiveSave()
{
	return USaveGameModule::Get().HasActiveSave();
}

FGuid USaveGameModuleStatics::GetActiveSaveId()
{
	return USaveGameModule::Get().GetActiveSaveId();
}

FSaveOperationResult USaveGameModuleStatics::SaveProfile(FName ProfileName)
{
	return USaveGameModule::Get().SaveProfile(ProfileName);
}

FSaveOperationResult USaveGameModuleStatics::LoadProfile(FName ProfileName)
{
	return USaveGameModule::Get().LoadProfile(ProfileName);
}
