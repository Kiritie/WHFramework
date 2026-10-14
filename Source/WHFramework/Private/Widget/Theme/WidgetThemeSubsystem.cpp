#include "Widget/Theme/WidgetThemeSubsystem.h"
#include "Widget/Theme/WidgetThemeSettings.h"
#include "Widget/Theme/WidgetThemeResolver.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "Engine/GameInstance.h"

bool UWidgetThemeSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	UGameInstance* GI = Cast<UGameInstance>(Outer);
	return Super::ShouldCreateSubsystem(Outer) && GI && !GI->IsDedicatedServerInstance();
}
void UWidgetThemeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	RequestTheme(GetDefault<UWidgetThemeSettings>()->DefaultTheme);
}
void UWidgetThemeSubsystem::Deinitialize()
{
	++RequestSerial;
	if (Pending)
	{
		Pending->CancelHandle();
	}
	Pending.Reset();
	ActiveTheme = nullptr;
	Super::Deinitialize();
}
void UWidgetThemeSubsystem::RequestTheme(TSoftObjectPtr<UWidgetTheme> Theme)
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
void UWidgetThemeSubsystem::CompleteRequest(uint64 Serial, TSoftObjectPtr<UWidgetTheme> Requested)
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

bool UWidgetThemeSubsystem::ApplyLoadedTheme(UWidgetTheme* Theme, FString& Error)
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
