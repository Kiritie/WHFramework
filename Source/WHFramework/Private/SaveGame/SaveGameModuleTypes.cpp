// Fill out your copyright notice in the Description page of Project Settings.


#include "SaveGame/SaveGameModuleTypes.h"

#include "Misc/Paths.h"

FSaveOperationResult FSaveOperationResult::Success()
{
	FSaveOperationResult Result;
	Result.bSuccess = true;
	Result.Code = ESaveResultCode::Success;
	return Result;
}

FSaveOperationResult FSaveOperationResult::Failed(ESaveResultCode InCode, const FText& InMessage)
{
	FSaveOperationResult Result;
	Result.Code = InCode;
	Result.Message = InMessage;
	return Result;
}

FSaveSlotSummary FSaveManifest::ToSummary(const FString& WorldDir) const
{
	FSaveSlotSummary Result;
	Result.SaveId = SaveId;
	Result.DisplayName = DisplayName;
	Result.Description = Description;
	Result.CreatedAt = CreatedAt;
	Result.UpdatedAt = UpdatedAt;
	Result.CurrentMap = CurrentMap;
	Result.PlayTimeSeconds = PlayTimeSeconds;
	Result.PreviewPath = PreviewFile.IsEmpty() ? FString() : FPaths::Combine(WorldDir, PreviewFile);
	Result.bValid = SaveId.IsValid();
	return Result;
}
