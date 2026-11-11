// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/LocalPlayer.h"
#include "Camera/CameraConfigTypes.h"
#include "GameplayTagContainer.h"

#include "WHLocalPlayer.generated.h"

class UInputMappingContext;
class UEnhancedPlayerInput;
class USettingModule;

struct FLocalInputContextLease
{
	TWeakObjectPtr<const UInputMappingContext> Context;
	int32 Priority = 0;
};

UCLASS()
class WHFRAMEWORK_API UWHLocalPlayer : public ULocalPlayer
{
	GENERATED_BODY()

public:
	UWHLocalPlayer();

	const FString& GetInputProfileId() const { return InputProfileId; }
	bool IsInputProfileApplied() const { return bInputProfileApplied; }
	bool SetWorldInputContext(UWorld* World, FGameplayTag Tag, const UInputMappingContext* Context, int32 Priority, bool bActive);
	bool HasWorldInputContext(const UWorld* World, FGameplayTag Tag) const;
	void ReleaseWorldInputContexts(UWorld* World);
	bool AcquireSettingEdit(const UObject* Owner, uint64& OutGeneration);
	bool OwnsSettingEdit(const UObject* Owner, uint64 Generation) const;
	bool IsSettingEditCurrent(const UObject* Owner, uint64 Generation) const;
	void ReleaseSettingEdit(const UObject* Owner, uint64 Generation);
	bool HasSettingEdit() const { return SettingEditOwner.IsValid(); }
	bool CanCaptureSettings() const;
	bool IsSettingEditOwner(const UObject* Owner) const { return Owner && SettingEditOwner.Get() == Owner; }
	void InvalidateSettingEditForProfileChange();
	void NoteExternalInputSetting(FName Mapping, uint8 Slot);
	void NoteExternalInputSettingRow(FName Mapping);
	void NoteExternalCameraSettings();
	bool CanRestoreInputSetting(FName Mapping, uint8 Slot) const;
	bool CanRestoreCameraSettings() const { return !bExternalCameraSettings; }
	bool RetryPendingSettingRecovery();
	void RetainSettingRecovery(USettingModule* Owner, uint64 Generation);
	void ClearSettingRecovery(const USettingModule* Owner);

private:
	friend class UInputModule;
	friend class UCameraModule;
	friend class UCameraSettingProvider;

	// An explicit local profile belongs to this LocalPlayer, not its current World or pawn.
	FString InputProfileId;
	bool bInputProfileApplied = false;
	bool bApplyingInputProfile = false;
	FName InputContainerProfile;
	TArray<uint8> DefaultInputSettings;
	TMap<FName, TArray<uint8>> InputProfileSessions;
	FCameraUserSettings LocalCameraSettings;
	bool bCameraProfileApplied = false;
	FName CameraContainerProfile;
	TMap<FName, FCameraUserSettings> CameraProfileSessions;
	TMap<TWeakObjectPtr<UWorld>, TMap<FGameplayTag, FLocalInputContextLease>> WorldInputContexts;
	TMap<TWeakObjectPtr<const UInputMappingContext>, int32> AppliedInputContexts;
	TWeakObjectPtr<UEnhancedPlayerInput> ManagedPlayerInput;
	TWeakObjectPtr<const UObject> SettingEditOwner;
	uint64 SettingEditGeneration = 0;
	bool bSettingEditInterrupted = false;
	bool bExternalCameraSettings = false;
	TSet<FString> ExternalInputSettings;
	TSet<FName> ExternalInputRows;
	UPROPERTY(Transient)
	TObjectPtr<USettingModule> PendingSettingRecovery;
	void SynchronizeInputContexts();
};
