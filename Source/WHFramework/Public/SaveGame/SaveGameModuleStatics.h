#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "SaveGame/SaveGameModuleTypes.h"

#include "SaveGameModuleStatics.generated.h"

UCLASS()
class WHFRAMEWORK_API USaveGameModuleStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "SaveGameModule")
	static int32 GetUserIndex();

	UFUNCTION(BlueprintCallable, Category = "SaveGameModule")
	static void SetUserIndex(int32 InUserIndex);

	UFUNCTION(BlueprintCallable, Category = "SaveGameModule")
	static FSaveOperationResult CreateSaveSlot(const FCreateSaveSlotParams& Params, FSaveSlotSummary& OutSummary);

	UFUNCTION(BlueprintCallable, Category = "SaveGameModule")
	static FSaveOperationResult SaveActiveSlot();

	UFUNCTION(BlueprintCallable, Category = "SaveGameModule")
	static void BeginPendingSaveSlot(const FCreateSaveSlotParams& Params);

	UFUNCTION(BlueprintCallable, Category = "SaveGameModule")
	static void CancelPendingSaveSlot();

	UFUNCTION(BlueprintPure, Category = "SaveGameModule")
	static bool HasPendingSaveSlot();

	UFUNCTION(BlueprintCallable, Category = "SaveGameModule")
	static FSaveOperationResult SaveCurrentSlot();

	UFUNCTION(BlueprintCallable, Category = "SaveGameModule")
	static void ClearActiveSave();

	UFUNCTION(BlueprintCallable, Category = "SaveGameModule")
	static FSaveOperationResult SaveSlot(FGuid SaveId);

	UFUNCTION(BlueprintCallable, Category = "SaveGameModule")
	static FSaveOperationResult LoadSaveSlot(FGuid SaveId, EPhase InPhase = EPhase::All);

	UFUNCTION(BlueprintCallable, Category = "SaveGameModule")
	static FSaveOperationResult DeleteSaveSlot(FGuid SaveId);

	UFUNCTION(BlueprintCallable, Category = "SaveGameModule")
	static FSaveOperationResult RenameSaveSlot(FGuid SaveId, const FString& NewName);

	UFUNCTION(BlueprintCallable, Category = "SaveGameModule")
	static FSaveOperationResult DuplicateSaveSlot(FGuid SourceSaveId, const FString& NewName, FSaveSlotSummary& OutSummary);

	UFUNCTION(BlueprintPure, Category = "SaveGameModule")
	static FSaveSlotSummary GetSaveSlotSummary(FGuid SaveId);

	UFUNCTION(BlueprintPure, Category = "SaveGameModule")
	static TArray<FSaveSlotSummary> GetSaveSlotSummaries();

	UFUNCTION(BlueprintPure, Category = "SaveGameModule")
	static bool HasActiveSave();

	UFUNCTION(BlueprintPure, Category = "SaveGameModule")
	static FGuid GetActiveSaveId();

	UFUNCTION(BlueprintCallable, Category = "SaveGameModule")
	static FSaveOperationResult SaveProfile(FName ProfileName = TEXT("Default"));

	UFUNCTION(BlueprintCallable, Category = "SaveGameModule")
	static FSaveOperationResult LoadProfile(FName ProfileName = TEXT("Default"));
};
