// Fill out your copyright notice in the Description page of Project Settings.


#include "Widget/WidgetModuleStatics.h"
#include "ObjectPool/ObjectPoolModuleTypes.h"
#include "Widget/WorldWidgetTypes.h"
#include "Common/LanguageTypes.h"

#include "Widget/Common/CommonToolTip.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerController.h"

namespace
{
UWidgetModule* WidgetModuleForContext(const UObject* InWorldContext)
{
    return InWorldContext ? UWidgetModule::GetPtr(InWorldContext->GetWorld()) : nullptr;
}
}

TArray<FLanguageType> UWidgetModuleStatics::GetWidgetLanguageTypes(const UObject* InWorldContext)
{ UWidgetModule* M = WidgetModuleForContext(InWorldContext); return M ? M->GetLanguageTypes() : TArray<FLanguageType>(); }
void UWidgetModuleStatics::SetWidgetLanguageTypes(const UObject* InWorldContext, const TArray<FLanguageType>& InLanguageTypes)
{ if (UWidgetModule* M = WidgetModuleForContext(InWorldContext)) M->SetLanguageTypes(InLanguageTypes); }
int32 UWidgetModuleStatics::GetWidgetLanguageType(const UObject* InWorldContext)
{ UWidgetModule* M = WidgetModuleForContext(InWorldContext); return M ? M->GetLanguageType() : INDEX_NONE; }
void UWidgetModuleStatics::SetWidgetLanguageType(const UObject* InWorldContext, int32 InLanguageType)
{ if (UWidgetModule* M = WidgetModuleForContext(InWorldContext)) M->SetLanguageType(InLanguageType); }
float UWidgetModuleStatics::GetWidgetGlobalScale(const UObject* InWorldContext)
{ UWidgetModule* M = WidgetModuleForContext(InWorldContext); return M ? M->GetGlobalScale() : 1.f; }
void UWidgetModuleStatics::SetWidgetGlobalScale(const UObject* InWorldContext, float InGlobalScale)
{ if (UWidgetModule* M = WidgetModuleForContext(InWorldContext)) M->SetGlobalScale(InGlobalScale); }
UDataTable* UWidgetModuleStatics::GetCommonRichTextStyle(const UObject* InWorldContext)
{ UWidgetModule* M = WidgetModuleForContext(InWorldContext); return M ? M->GetCommonRichTextStyle() : nullptr; }
void UWidgetModuleStatics::SetCommonRichTextStyle(const UObject* InWorldContext, UDataTable* InCommonRichTextStyle)
{ if (UWidgetModule* M = WidgetModuleForContext(InWorldContext)) M->SetCommonRichTextStyle(InCommonRichTextStyle); }
TArray<TSubclassOf<URichTextBlockDecorator>> UWidgetModuleStatics::GetCommonRichTextDecorators(const UObject* InWorldContext)
{ UWidgetModule* M = WidgetModuleForContext(InWorldContext); return M ? M->GetCommonRichTextDecorators() : TArray<TSubclassOf<URichTextBlockDecorator>>(); }
void UWidgetModuleStatics::SetCommonRichTextDecorators(const UObject* InWorldContext, const TArray<TSubclassOf<URichTextBlockDecorator>>& InCommonRichTextDecorators)
{ if (UWidgetModule* M = WidgetModuleForContext(InWorldContext)) M->SetCommonRichTextDecorators(InCommonRichTextDecorators); }
void UWidgetModuleStatics::AddCommonRichTextDecorator(const UObject* InWorldContext, const TSubclassOf<URichTextBlockDecorator>& InCommonRichTextDecorator)
{ if (UWidgetModule* M = WidgetModuleForContext(InWorldContext)) M->AddCommonRichTextDecorator(InCommonRichTextDecorator); }
void UWidgetModuleStatics::RemoveCommonRichTextDecorator(const UObject* InWorldContext, const TSubclassOf<URichTextBlockDecorator>& InCommonRichTextDecorator)
{ if (UWidgetModule* M = WidgetModuleForContext(InWorldContext)) M->RemoveCommonRichTextDecorator(InCommonRichTextDecorator); }
UCommonToolTip* UWidgetModuleStatics::SpawnToolTipForWidget(UWidget* InWidget, TSubclassOf<UCommonToolTip> InToolTipClass)
{
	UObjectPoolModule* Pool = InWidget ? UObjectPoolModule::GetPtr(InWidget->GetWorld()) : nullptr;
	if (!Pool) return nullptr;
	FWidgetSpawnParameter Spawn(InWidget);
	if (const UUserWidget* Owner = InWidget->GetTypedOuter<UUserWidget>()) Spawn.OwningPlayer = Owner->GetOwningPlayer();
	return Pool->SpawnObject<UCommonToolTip>(Spawn, InToolTipClass);
}

void UWidgetModuleStatics::DestroyToolTipForWidget(UWidget* InWidget)
{
	if (UObjectPoolModule* Pool = InWidget ? UObjectPoolModule::GetPtr(InWidget->GetWorld()) : nullptr)
		Pool->DespawnObject(InWidget->GetToolTip());
}

UWidgetModule* UWidgetModuleStatics::GetPlayerModule(APlayerController* InPlayer)
{
    return ::IsValid(InPlayer) && InPlayer->IsLocalController() ? UWidgetModule::GetPtr(InPlayer->GetWorld()) : nullptr;
}

UUserWidgetBase* UWidgetModuleStatics::GetUserWidgetByTag(APlayerController* InPlayer, FGameplayTag InWidgetTag, TSubclassOf<UUserWidgetBase> InExpectedClass)
{ UWidgetModule* M = GetPlayerModule(InPlayer); return M ? M->GetUserWidgetByTag(InPlayer, InWidgetTag, InExpectedClass) : nullptr; }
UUserWidgetBase* UWidgetModuleStatics::CreateUserWidgetByTag(APlayerController* InPlayer, FGameplayTag InWidgetTag, const FParameter& InParam, TSubclassOf<UUserWidgetBase> InClass)
{ UWidgetModule* M = GetPlayerModule(InPlayer); return M ? M->CreateUserWidgetByTag(InPlayer, InWidgetTag, InParam, InClass) : nullptr; }
bool UWidgetModuleStatics::OpenUserWidgetByTag(APlayerController* InPlayer, FGameplayTag InWidgetTag, const FParameter& InParam, bool bInstant, bool bForce)
{ UWidgetModule* M = GetPlayerModule(InPlayer); return M && M->OpenUserWidgetByTag(InPlayer, InWidgetTag, InParam, bInstant, bForce); }
bool UWidgetModuleStatics::CloseUserWidgetByTag(APlayerController* InPlayer, FGameplayTag InWidgetTag, bool bInstant)
{ UWidgetModule* M = GetPlayerModule(InPlayer); return M && M->CloseUserWidgetByTag(InPlayer, InWidgetTag, bInstant); }
bool UWidgetModuleStatics::ToggleUserWidgetByTag(APlayerController* InPlayer, FGameplayTag InWidgetTag, bool bInstant)
{ UWidgetModule* M = GetPlayerModule(InPlayer); return M && M->ToggleUserWidgetByTag(InPlayer, InWidgetTag, bInstant); }
bool UWidgetModuleStatics::DestroyUserWidgetByTag(APlayerController* InPlayer, FGameplayTag InWidgetTag, EObjectDespawnMode InMode)
{ UWidgetModule* M = GetPlayerModule(InPlayer); return M && M->DestroyUserWidgetByTag(InPlayer, InWidgetTag, InMode); }
bool UWidgetModuleStatics::HasUserWidgetClass(APlayerController* InPlayer, TSubclassOf<UUserWidgetBase> InClass)
{ UWidgetModule* M = GetPlayerModule(InPlayer); return M && M->HasUserWidgetClass(InClass); }
TSubclassOf<UUserWidgetBase> UWidgetModuleStatics::GetUserWidgetClass(APlayerController* InPlayer, TSubclassOf<UUserWidgetBase> InClass)
{ UWidgetModule* M = GetPlayerModule(InPlayer); return M ? M->GetUserWidgetClass(InClass) : nullptr; }
bool UWidgetModuleStatics::HasUserWidget(APlayerController* InPlayer, TSubclassOf<UUserWidgetBase> InClass)
{ UWidgetModule* M = GetPlayerModule(InPlayer); return M && M->HasUserWidget(InPlayer, InClass); }
UUserWidgetBase* UWidgetModuleStatics::GetUserWidget(APlayerController* InPlayer, TSubclassOf<UUserWidgetBase> InClass)
{ UWidgetModule* M = GetPlayerModule(InPlayer); return M ? M->GetUserWidget(InPlayer, InClass) : nullptr; }
UUserWidgetBase* UWidgetModuleStatics::CreateUserWidget(APlayerController* InPlayer, TSubclassOf<UUserWidgetBase> InClass, const FParameter& InParam)
{ UWidgetModule* M = GetPlayerModule(InPlayer); return M ? M->CreateUserWidget(InPlayer, InClass, InParam) : nullptr; }
bool UWidgetModuleStatics::OpenUserWidget(APlayerController* InPlayer, TSubclassOf<UUserWidgetBase> InClass, const FParameter& InParam, bool bInstant, bool bForce)
{ UWidgetModule* M = GetPlayerModule(InPlayer); return M && M->OpenUserWidget(InPlayer, InClass, InParam, bInstant, bForce); }
bool UWidgetModuleStatics::CloseUserWidget(APlayerController* InPlayer, TSubclassOf<UUserWidgetBase> InClass, bool bInstant)
{ UWidgetModule* M = GetPlayerModule(InPlayer); return M && M->CloseUserWidget(InPlayer, InClass, bInstant); }
bool UWidgetModuleStatics::ToggleUserWidget(APlayerController* InPlayer, TSubclassOf<UUserWidgetBase> InClass, bool bInstant)
{ UWidgetModule* M = GetPlayerModule(InPlayer); return M && M->ToggleUserWidget(InPlayer, InClass, bInstant); }
bool UWidgetModuleStatics::DestroyUserWidget(APlayerController* InPlayer, TSubclassOf<UUserWidgetBase> InClass, EObjectDespawnMode InMode)
{ UWidgetModule* M = GetPlayerModule(InPlayer); return M && M->DestroyUserWidget(InPlayer, InClass, InMode); }
void UWidgetModuleStatics::CloseAllUserWidget(APlayerController* InPlayer, bool bInstant)
{ if (UWidgetModule* M = GetPlayerModule(InPlayer)) M->CloseAllUserWidget(InPlayer, bInstant); }

void UWidgetModuleStatics::CloseAllSlateWidget(bool bInstant)
{
	FSlateWidgetManager::Get().CloseAllSlateWidget(bInstant);
}

UWorldWidgetContainer* UWidgetModuleStatics::GetWorldWidgetContainer(APlayerController* InPlayer)
{
	UWidgetModule* Widgets = GetPlayerModule(InPlayer);
	return Widgets ? Widgets->GetWorldWidgetContainer(InPlayer) : nullptr;
}

TArray<UWorldWidgetBase*> UWidgetModuleStatics::GetWorldWidgetsByTag(const UObject* InWorldContext, FGameplayTag InWidgetTag)
{ UWidgetModule* M = WidgetModuleForContext(InWorldContext); return M ? M->GetWorldWidgetsByTag(InWidgetTag) : TArray<UWorldWidgetBase*>(); }
UWorldWidgetBase* UWidgetModuleStatics::CreateWorldWidgetByTag(const UObject* InWorldContext, FGameplayTag InWidgetTag, FWorldWidgetMapping InMapping, const FParameter& InParam, TSubclassOf<UWorldWidgetBase> InClass)
{ UWidgetModule* M = WidgetModuleForContext(InWorldContext); return M ? M->CreateWorldWidgetByTag(InWidgetTag, InMapping, InParam, InClass) : nullptr; }
bool UWidgetModuleStatics::DestroyWorldWidgetByTag(const UObject* InWorldContext, FGameplayTag InWidgetTag, UWorldWidgetBase* InWidget, EObjectDespawnMode InMode)
{ UWidgetModule* M = WidgetModuleForContext(InWorldContext); return M && InWidget && M->DestroyWorldWidgetByTag(InWidgetTag, InWidget, InMode); }
bool UWidgetModuleStatics::GetWorldWidgetVisibleByTag(const UObject* InWorldContext, FGameplayTag InWidgetTag)
{ UWidgetModule* M = WidgetModuleForContext(InWorldContext); return M && M->GetWorldWidgetVisibleByTag(InWidgetTag); }
void UWidgetModuleStatics::SetWorldWidgetVisibleByTag(const UObject* InWorldContext, FGameplayTag InWidgetTag, bool bVisible)
{ if (UWidgetModule* M = WidgetModuleForContext(InWorldContext)) M->SetWorldWidgetVisibleByTag(InWidgetTag, bVisible); }
bool UWidgetModuleStatics::GetWorldWidgetVisible(const UObject* InWorldContext, TSubclassOf<UWorldWidgetBase> InClass)
{ UWidgetModule* M = WidgetModuleForContext(InWorldContext); return M && M->GetWorldWidgetVisible(InClass); }
void UWidgetModuleStatics::SetWorldWidgetVisible(const UObject* InWorldContext, bool bVisible, TSubclassOf<UWorldWidgetBase> InClass)
{ if (UWidgetModule* M = WidgetModuleForContext(InWorldContext)) M->SetWorldWidgetVisible(bVisible, InClass); }
TArray<UWorldWidgetBase*> UWidgetModuleStatics::GetWorldWidgets(const UObject* InWorldContext, TSubclassOf<UWorldWidgetBase> InClass)
{ UWidgetModule* M = WidgetModuleForContext(InWorldContext); return M ? M->GetWorldWidgets(InClass) : TArray<UWorldWidgetBase*>(); }
UWorldWidgetBase* UWidgetModuleStatics::CreateWorldWidget(const UObject* InWorldContext, TSubclassOf<UWorldWidgetBase> InClass, FWorldWidgetMapping InMapping, const FParameter& InParam)
{ UWidgetModule* M = WidgetModuleForContext(InWorldContext); return M ? M->CreateWorldWidget(InClass, InMapping, InParam) : nullptr; }
void UWidgetModuleStatics::DestroyWorldWidgets(const UObject* InWorldContext, TSubclassOf<UWorldWidgetBase> InClass, EObjectDespawnMode InMode)
{ if (UWidgetModule* M = WidgetModuleForContext(InWorldContext)) M->DestroyWorldWidgets(InClass, InMode); }
