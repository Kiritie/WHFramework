#pragma once

#include "Main/Base/ModuleBase.h"
#include "SaveGame/SaveGameModuleTypes.h"
#include "SaveGame/SaveGameStorage.h"

#include "SaveGameModule.generated.h"


UCLASS()
class WHFRAMEWORK_API USaveGameModule : public UModuleBase
{
	GENERATED_BODY()

	GENERATED_MODULE(USaveGameModule)

public:
	USaveGameModule();
	~USaveGameModule();

	//////////////////////////////////////////////////////////////////////////
public:
	virtual void OnInitialize() override;
	virtual void OnPreparatory(EPhase InPhase) override;
	virtual void OnTermination(EPhase InPhase) override;

protected:
	virtual void LoadData(const FParameter& InSaveData, EPhase InPhase) override;
	virtual FParameter ToData() override;

public:
	UFUNCTION(BlueprintCallable, Category = "SaveGame|World")
	FSaveOperationResult CreateSaveSlot(const FCreateSaveSlotParams& Params, FSaveSlotSummary& OutSummary);

	UFUNCTION(BlueprintCallable, Category = "SaveGame|World")
	FSaveOperationResult SaveSlot(FGuid SaveId);

	UFUNCTION(BlueprintCallable, Category = "SaveGame|World")
	FSaveOperationResult SaveActiveSlot();

	UFUNCTION(BlueprintCallable, Category = "SaveGame|World")
	FSaveOperationResult LoadSlot(FGuid SaveId);

	UFUNCTION(BlueprintCallable, Category = "SaveGame|World")
	FSaveOperationResult DeleteSaveSlot(FGuid SaveId);

	UFUNCTION(BlueprintCallable, Category = "SaveGame|World")
	FSaveOperationResult RenameSaveSlot(FGuid SaveId, const FString& NewName);

	UFUNCTION(BlueprintCallable, Category = "SaveGame|World")
	FSaveOperationResult DuplicateSaveSlot(FGuid SourceSaveId, const FString& NewName, FSaveSlotSummary& OutSummary);

	UFUNCTION(BlueprintPure, Category = "SaveGame|World")
	bool HasActiveSave() const { return ActiveSaveId.IsValid(); }

	UFUNCTION(BlueprintPure, Category = "SaveGame|World")
	FGuid GetActiveSaveId() const { return ActiveSaveId; }

	UFUNCTION(BlueprintPure, Category = "SaveGame|World")
	FSaveSlotSummary GetSaveSlotSummary(FGuid SaveId) const;

	UFUNCTION(BlueprintPure, Category = "SaveGame|World")
	TArray<FSaveSlotSummary> GetSaveSlotSummaries() const;

	UFUNCTION(BlueprintCallable, Category = "SaveGame|Profile")
	FSaveOperationResult SaveProfile(FName ProfileName = TEXT("Default"));

	UFUNCTION(BlueprintCallable, Category = "SaveGame|Profile")
	FSaveOperationResult LoadProfile(FName ProfileName = TEXT("Default"));

	UFUNCTION(BlueprintPure)
	int32 GetUserIndex() const { return UserIndex; }

	UFUNCTION(BlueprintCallable)
	void SetUserIndex(int32 InUserIndex);

private:
	FSaveOperationResult SaveSlotInternal(FGuid SaveId);
	FSaveOperationResult CaptureModulesToGeneration(const FGuid& SaveId, int32 Generation, TArray<UModuleBase*>& OutCaptured);
	FSaveOperationResult LoadModulesFromGeneration(const FGuid& SaveId, int32 Generation);
	FSaveOperationResult RestoreSlotGeneration(const FPendingSaveLoadContext& Context);
	TArray<UModuleBase*> GetSaveModules(ESaveScope Scope) const;
	void OnGameExited(UObject* InSender, const struct FEventGameExited& InEvent);

private:
	UPROPERTY(EditAnywhere, Category = "UserData")
	int32 UserIndex;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SaveGame", meta = (AllowPrivateAccess = "true"))
	FGuid ActiveSaveId;

	UPROPERTY(Transient)
	bool bSaveOperationRunning;

	UPROPERTY(Transient)
	FPendingSaveLoadContext PendingLoadContext;

	TUniquePtr<FSaveGameStorage> Storage;
};
