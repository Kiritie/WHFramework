#pragma once

#include "SaveGame/SaveOperationTypes.h"
#include "SaveGame/SaveSlotTypes.h"
#include "Common/LifecycleTypes.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "SaveGame/SaveGameModuleTypes.h"

#include "SaveGameModuleStatics.generated.h"


UCLASS()
class WHFRAMEWORK_API USaveGameModuleStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "SaveGameModule", meta = (WorldContext = "WorldContextObject"))
	static int32 GetUserIndex(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "SaveGameModule", meta = (WorldContext = "WorldContextObject"))
	static void SetUserIndex(const UObject* WorldContextObject, int32 InUserIndex);

	UFUNCTION(BlueprintCallable, Category = "SaveGameModule", meta = (WorldContext = "WorldContextObject"))
	static FSaveOperationResult CreateSaveSlot(const UObject* WorldContextObject, const FCreateSaveSlotParams& Params, FSaveSlotSummary& OutSummary);

	UFUNCTION(BlueprintCallable, Category = "SaveGameModule", meta = (WorldContext = "WorldContextObject"))
	static FSaveOperationResult SaveActiveSlot(const UObject* WorldContextObject);

	static FGuid BeginPendingSaveSlot(const UObject* WorldContextObject, const FCreateSaveSlotParams& Params);
	static bool CancelPendingSaveSlot(const UObject* WorldContextObject, const FGuid& InToken);
	static bool IsPendingSaveSlot(const UObject* WorldContextObject, const FGuid& InToken);
	static FSaveOperationResult SavePendingSlot(const UObject* WorldContextObject, const FGuid& InToken);
	static bool ConsumePendingSlotResult(const UObject* WorldContextObject, const FGuid& InToken, FSaveOperationResult& OutResult);

	UFUNCTION(BlueprintPure, Category = "SaveGameModule", meta = (WorldContext = "WorldContextObject"))
	static bool HasPendingSaveSlot(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "SaveGameModule", meta = (WorldContext = "WorldContextObject"))
	static FSaveOperationResult SaveCurrentSlot(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "SaveGameModule", meta = (WorldContext = "WorldContextObject"))
	static FSaveOperationResult GetWorldSaveResult(const UObject* WorldContextObject, const FGuid& InOperationId);

	UFUNCTION(BlueprintCallable, Category = "SaveGameModule", meta = (WorldContext = "WorldContextObject"))
	static void DetachActiveSave(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "SaveGameModule", meta = (WorldContext = "WorldContextObject"))
	static void ClearActiveSave(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "SaveGameModule", meta = (WorldContext = "WorldContextObject"))
	static FSaveOperationResult SaveSlot(const UObject* WorldContextObject, FGuid SaveId);

	UFUNCTION(BlueprintCallable, Category = "SaveGameModule", meta = (WorldContext = "WorldContextObject"))
	static FSaveOperationResult LoadSaveSlot(const UObject* WorldContextObject, FGuid SaveId, EPhase InPhase = EPhase::All);

	UFUNCTION(BlueprintCallable, Category = "SaveGameModule", meta = (WorldContext = "WorldContextObject"))
	static FSaveOperationResult DeleteSaveSlot(const UObject* WorldContextObject, FGuid SaveId);

	UFUNCTION(BlueprintCallable, Category = "SaveGameModule", meta = (WorldContext = "WorldContextObject"))
	static FSaveOperationResult RenameSaveSlot(const UObject* WorldContextObject, FGuid SaveId, const FString& NewName);

	UFUNCTION(BlueprintCallable, Category = "SaveGameModule", meta = (WorldContext = "WorldContextObject"))
	static FSaveOperationResult DuplicateSaveSlot(const UObject* WorldContextObject, FGuid SourceSaveId, const FString& NewName, FSaveSlotSummary& OutSummary);

	UFUNCTION(BlueprintPure, Category = "SaveGameModule", meta = (WorldContext = "WorldContextObject"))
	static FSaveSlotSummary GetSaveSlotSummary(const UObject* WorldContextObject, FGuid SaveId);

	UFUNCTION(BlueprintPure, Category = "SaveGameModule", meta = (WorldContext = "WorldContextObject"))
	static TArray<FSaveSlotSummary> GetSaveSlotSummaries(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "SaveGameModule", meta = (WorldContext = "WorldContextObject"))
	static bool HasActiveSave(const UObject* WorldContextObject);

	UFUNCTION(BlueprintPure, Category = "SaveGameModule", meta = (WorldContext = "WorldContextObject"))
	static FGuid GetActiveSaveId(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "SaveGameModule", meta = (WorldContext = "WorldContextObject"))
	static FSaveOperationResult SaveProfile(const UObject* WorldContextObject, FName ProfileName = TEXT("Default"));

	UFUNCTION(BlueprintCallable, Category = "SaveGameModule", meta = (WorldContext = "WorldContextObject"))
	static FSaveOperationResult LoadProfile(const UObject* WorldContextObject, FName ProfileName = TEXT("Default"));
};
