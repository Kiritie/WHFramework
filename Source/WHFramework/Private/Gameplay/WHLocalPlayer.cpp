// Copyright Epic Games, Inc. All Rights Reserved.

#include "Gameplay/WHLocalPlayer.h"
#include "Setting/SettingModule.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedPlayerInput.h"
#include "InputMappingContext.h"
#include "Engine/World.h"

bool UWHLocalPlayer::AcquireSettingEdit(const UObject* Owner, uint64& OutGeneration)
{
	if (!::IsValid(Owner) || !RetryPendingSettingRecovery() || SettingEditOwner.IsValid() || bApplyingInputProfile) return false;
	SettingEditOwner = Owner;
	bSettingEditInterrupted = false;
	bExternalCameraSettings = false;
	ExternalInputSettings.Reset();
	ExternalInputRows.Reset();
	OutGeneration = ++SettingEditGeneration;
	return true;
}

bool UWHLocalPlayer::OwnsSettingEdit(const UObject* Owner, uint64 Generation) const
{
	return ::IsValid(Owner) && SettingEditOwner.Get() == Owner && SettingEditGeneration == Generation;
}

bool UWHLocalPlayer::CanCaptureSettings() const
{
	const USettingModule* Settings = Cast<USettingModule>(SettingEditOwner.Get());
	return !Settings || !Settings->HasUncommittedAppliedSettings();
}

bool UWHLocalPlayer::IsSettingEditCurrent(const UObject* Owner, uint64 Generation) const
{
	return OwnsSettingEdit(Owner, Generation) && !bSettingEditInterrupted;
}

void UWHLocalPlayer::NoteExternalInputSetting(FName Mapping, uint8 Slot)
{
	if (!HasSettingEdit()) return;
	bSettingEditInterrupted = true;
	ExternalInputSettings.Add(Mapping.ToString() + TEXT(":") + FString::FromInt(Slot));
}

void UWHLocalPlayer::NoteExternalInputSettingRow(FName Mapping)
{
	if (!HasSettingEdit()) return;
	bSettingEditInterrupted = true;
	ExternalInputRows.Add(Mapping);
}

void UWHLocalPlayer::NoteExternalCameraSettings()
{
	if (!HasSettingEdit()) return;
	bSettingEditInterrupted = true;
	bExternalCameraSettings = true;
}

bool UWHLocalPlayer::CanRestoreInputSetting(FName Mapping, uint8 Slot) const
{
	return !ExternalInputRows.Contains(Mapping) && !ExternalInputSettings.Contains(Mapping.ToString() + TEXT(":") + FString::FromInt(Slot));
}

void UWHLocalPlayer::ReleaseSettingEdit(const UObject* Owner, uint64 Generation)
{
	if (OwnsSettingEdit(Owner, Generation)) SettingEditOwner.Reset();
}

void UWHLocalPlayer::InvalidateSettingEditForProfileChange()
{
	PendingSettingRecovery = nullptr;
	SettingEditOwner.Reset();
	++SettingEditGeneration;
}

bool UWHLocalPlayer::RetryPendingSettingRecovery()
{
	return !PendingSettingRecovery || PendingSettingRecovery->RetryHostTerminationRecovery();
}

void UWHLocalPlayer::RetainSettingRecovery(USettingModule* Owner, uint64 Generation)
{
	if (OwnsSettingEdit(Owner, Generation)) PendingSettingRecovery = Owner;
}

void UWHLocalPlayer::ClearSettingRecovery(const USettingModule* Owner)
{
	if (PendingSettingRecovery == Owner) PendingSettingRecovery = nullptr;
}
UWHLocalPlayer::UWHLocalPlayer()
{
	
}

bool UWHLocalPlayer::SetWorldInputContext(UWorld* World, FGameplayTag Tag, const UInputMappingContext* Context, int32 Priority, bool bActive)
{
	UEnhancedInputLocalPlayerSubsystem* Input = GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	if (!IsValid(World) || !Tag.IsValid() || !IsValid(Context) || !Input) return false;
	int32 ExistingPriority = INDEX_NONE;
	if (bActive && Input->GetPlayerInput() && !AppliedInputContexts.Contains(Context) &&
		Input->HasMappingContext(Context, ExistingPriority) && ExistingPriority != Priority)
	{
		UE_LOG(LogTemp, Error, TEXT("Input context %s is already owned at a different priority."), *GetNameSafe(Context));
		return false;
	}
	if (bActive)
	{
		FLocalInputContextLease& Lease = WorldInputContexts.FindOrAdd(World).FindOrAdd(Tag);
		Lease.Context = Context;
		Lease.Priority = Priority;
	}
	else if (TMap<FGameplayTag, FLocalInputContextLease>* Leases = WorldInputContexts.Find(World))
	{
		Leases->Remove(Tag);
		if (Leases->IsEmpty()) WorldInputContexts.Remove(World);
	}
	SynchronizeInputContexts();
	return true;
}

bool UWHLocalPlayer::HasWorldInputContext(const UWorld* World, FGameplayTag Tag) const
{
	const TMap<FGameplayTag, FLocalInputContextLease>* Leases = WorldInputContexts.Find(const_cast<UWorld*>(World));
	const FLocalInputContextLease* Lease = Leases ? Leases->Find(Tag) : nullptr;
	const UEnhancedInputLocalPlayerSubsystem* Input = GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
	int32 Priority = INDEX_NONE;
	return Lease && Lease->Context.IsValid() && Input && Input->HasMappingContext(Lease->Context.Get(), Priority) && Priority >= Lease->Priority;
}

void UWHLocalPlayer::ReleaseWorldInputContexts(UWorld* World)
{
	if (WorldInputContexts.Remove(World) > 0) SynchronizeInputContexts();
}

void UWHLocalPlayer::SynchronizeInputContexts()
{
    UEnhancedInputLocalPlayerSubsystem* Input = GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
    UEnhancedPlayerInput* PlayerInput = Input ? Input->GetPlayerInput() : nullptr;
    if (ManagedPlayerInput.Get() != PlayerInput)
    {
        ManagedPlayerInput = PlayerInput;
        AppliedInputContexts.Reset();
    }
    if (!Input || !PlayerInput) return;
    TMap<TWeakObjectPtr<const UInputMappingContext>, int32> Desired;
    for (auto It = WorldInputContexts.CreateIterator(); It; ++It)
    {
        if (!It.Key().IsValid()) { It.RemoveCurrent(); continue; }
        for (const auto& Pair : It.Value())
        {
            if (!Pair.Value.Context.IsValid()) continue;
            int32* Priority = Desired.Find(Pair.Value.Context);
            if (Priority) *Priority = FMath::Max(*Priority, Pair.Value.Priority);
            else Desired.Add(Pair.Value.Context, Pair.Value.Priority);
        }
    }
    const auto Previous = AppliedInputContexts;
    for (const auto& Pair : Previous)
    {
        const int32* Priority = Desired.Find(Pair.Key);
        if (!Pair.Key.IsValid() || !Priority || *Priority != Pair.Value || !Input->HasMappingContext(Pair.Key.Get()))
        {
            AppliedInputContexts.Remove(Pair.Key);
            if (Pair.Key.IsValid() && Input->HasMappingContext(Pair.Key.Get())) Input->RemoveMappingContext(Pair.Key.Get());
        }
    }
    for (const auto& Pair : Desired)
    {
        if (!AppliedInputContexts.Contains(Pair.Key) && !Input->HasMappingContext(Pair.Key.Get()))
        {
            AppliedInputContexts.Add(Pair.Key, Pair.Value);
            Input->AddMappingContext(Pair.Key.Get(), Pair.Value);
        }
    }
}
