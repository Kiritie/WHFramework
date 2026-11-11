#include "Widget/Theme/WidgetThemeModule.h"
#include "Common/LifecycleTypes.h"
#include "Widget/Theme/WidgetThemeSettings.h"
#include "Widget/Theme/WidgetThemeResolver.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Gameplay/WHGameInstance.h"

UWidgetThemeModule::UWidgetThemeModule()
{
	ModuleName = TEXT("WidgetThemeModule");
}

bool UWidgetThemeModule::ShouldCreateForGameInstance(const UWHGameInstance* InGameInstance) const
{
	return InGameInstance && !InGameInstance->IsDedicatedServerInstance();
}

UWidgetThemeModule* UWidgetThemeModule::GetPtr(const UGameInstance* InGameInstance)
{
	const UWHGameInstance* Owner = Cast<UWHGameInstance>(InGameInstance);
	return Owner ? Owner->FindModule<UWidgetThemeModule>() : nullptr;
}

UWidgetThemeModule* UWidgetThemeModule::GetPtr(const UWorld* InWorld)
{
	return InWorld ? GetPtr(InWorld->GetGameInstance()) : nullptr;
}

void UWidgetThemeModule::OnInitialize()
{
	Super::OnInitialize();
	RequestTheme(GetDefault<UWidgetThemeSettings>()->DefaultTheme);
}

void UWidgetThemeModule::OnTermination(EPhase InPhase)
{
	if (InPhase == EPhase::Final)
	{
		++RequestSerial;
		if (Pending)
		{
			Pending->CancelHandle();
		}
		Pending.Reset();
		ActiveTheme = nullptr;
	}
	Super::OnTermination(InPhase);
}
void UWidgetThemeModule::RequestTheme(TSoftObjectPtr<UWidgetTheme> Theme)
{
	check(IsInGameThread());
	const uint64 Serial = ++RequestSerial;
	if (Pending)
	{
		Pending->CancelHandle();
	}
	Pending.Reset();
	const auto* Settings = GetDefault<UWidgetThemeSettings>();
	if (Theme.IsNull() || (Theme != Settings->DefaultTheme && !Settings->AvailableThemes.Contains(Theme)))
	{
		OnSwitchFinished.Broadcast(false, TEXT("Theme is empty or not in the configured cook allowlist."));
		return;
	}
	TArray<FSoftObjectPath> Paths{Theme.ToSoftObjectPath()};
	Pending =
		UAssetManager::GetStreamableManager().RequestAsyncLoad(Paths, FStreamableDelegate::CreateUObject(this, &ThisClass::CompleteRequest, Serial, Theme));
	if (!Pending)
	{
		OnSwitchFinished.Broadcast(false, TEXT("Unable to create theme load request."));
	}
}
void UWidgetThemeModule::CompleteRequest(uint64 Serial, TSoftObjectPtr<UWidgetTheme> Requested)
{
	if (Serial != RequestSerial)
	{
		return;
	}
	UWidgetTheme* Candidate = Requested.Get();
	FString Error;
	if (Candidate && !Candidate->bRuntimeTheme)
	{
		Pending.Reset();
		OnSwitchFinished.Broadcast(false, TEXT("A library theme cannot be activated as a runtime theme."));
		return;
	}
	if (!FWidgetThemeResolver::Validate(Candidate, Error))
	{
		Pending.Reset();
		UE_LOG(LogTemp, Error, TEXT("Theme switch rejected: %s"), *Error);
		OnSwitchFinished.Broadcast(false, Error);
		return;
	}

	ActiveTheme = Candidate;
	Pending.Reset();
	FWidgetThemeResolver::NotifyThemeChanged();
	OnSwitchFinished.Broadcast(true, FString());
}

bool UWidgetThemeModule::ApplyLoadedTheme(UWidgetTheme* Theme, FString& Error)
{
	check(IsInGameThread());
	++RequestSerial;
	if (Pending)
	{
		Pending->CancelHandle();
	}
	Pending.Reset();
	const auto* Settings = GetDefault<UWidgetThemeSettings>();
	const TSoftObjectPtr<UWidgetTheme> Reference(Theme);
	if (!Theme || (Reference != Settings->DefaultTheme && !Settings->AvailableThemes.Contains(Reference)))
	{
		Error = TEXT("Loaded theme is empty or not in the configured cook allowlist.");
		OnSwitchFinished.Broadcast(false, Error);
		return false;
	}
	if (!Theme->bRuntimeTheme)
	{
		Error = TEXT("A library theme cannot be activated as a runtime theme.");
		OnSwitchFinished.Broadcast(false, Error);
		return false;
	}
	if (!FWidgetThemeResolver::Validate(Theme, Error))
	{
		OnSwitchFinished.Broadcast(false, Error);
		return false;
	}
	ActiveTheme = Theme;
	FWidgetThemeResolver::NotifyThemeChanged();
	OnSwitchFinished.Broadcast(true, FString());
	return true;
}
