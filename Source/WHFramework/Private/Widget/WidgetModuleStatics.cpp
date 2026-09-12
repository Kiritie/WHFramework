// Fill out your copyright notice in the Description page of Project Settings.


#include "Widget/WidgetModuleStatics.h"

#include "Widget/Common/CommonToolTip.h"

TArray<FLanguageType> UWidgetModuleStatics::GetWidgetLanguageTypes()
{
	return UWidgetModule::Get().GetLanguageTypes();
}

void UWidgetModuleStatics::SetWidgetLanguageTypes(const TArray<FLanguageType>& InLanguageTypes)
{
	UWidgetModule::Get().SetLanguageTypes(InLanguageTypes);
}

int32 UWidgetModuleStatics::GetWidgetLanguageType()
{
	return UWidgetModule::Get().GetLanguageType();
}

void UWidgetModuleStatics::SetWidgetLanguageType(int32 InLanguageType)
{
	UWidgetModule::Get().SetLanguageType(InLanguageType);
}

float UWidgetModuleStatics::GetWidgetGlobalScale()
{
	return UWidgetModule::Get().GetGlobalScale();
}

void UWidgetModuleStatics::SetWidgetGlobalScale(float InGlobalScale)
{
	UWidgetModule::Get().SetGlobalScale(InGlobalScale);
}

UDataTable* UWidgetModuleStatics::GetCommonRichTextStyle()
{
	return UWidgetModule::Get().GetCommonRichTextStyle();
}

void UWidgetModuleStatics::SetCommonRichTextStyle(UDataTable* InCommonRichTextStyle)
{
	UWidgetModule::Get().SetCommonRichTextStyle(InCommonRichTextStyle);
}

TArray<TSubclassOf<URichTextBlockDecorator>> UWidgetModuleStatics::GetCommonRichTextDecorators()
{
	return UWidgetModule::Get().GetCommonRichTextDecorators();
}

void UWidgetModuleStatics::SetCommonRichTextDecorators(const TArray<TSubclassOf<URichTextBlockDecorator>>& InCommonRichTextDecorators)
{
	UWidgetModule::Get().SetCommonRichTextDecorators(InCommonRichTextDecorators);
}

void UWidgetModuleStatics::AddCommonRichTextDecorator(const TSubclassOf<URichTextBlockDecorator>& InCommonRichTextDecorator)
{
	UWidgetModule::Get().AddCommonRichTextDecorator(InCommonRichTextDecorator);
}

void UWidgetModuleStatics::RemoveCommonRichTextDecorator(const TSubclassOf<URichTextBlockDecorator>& InCommonRichTextDecorator)
{
	UWidgetModule::Get().RemoveCommonRichTextDecorator(InCommonRichTextDecorator);
}

UCommonToolTip* UWidgetModuleStatics::SpawnToolTipForWidget(UWidget* InWidget, TSubclassOf<UCommonToolTip> InToolTipClass)
{
	return UObjectPoolModuleStatics::SpawnObject<UCommonToolTip>(
		FWidgetSpawnParameter(InWidget),
		InToolTipClass);
}

void UWidgetModuleStatics::DestroyToolTipForWidget(UWidget* InWidget)
{
	UObjectPoolModuleStatics::DespawnObject(InWidget->GetToolTip());
}

UUserWidgetBase* UWidgetModuleStatics::GetUserWidgetByTag(FGameplayTag InWidgetTag, TSubclassOf<UUserWidgetBase> InExpectedClass)
{
	return UWidgetModule::Get().GetUserWidgetByTag(InWidgetTag, InExpectedClass);
}

UUserWidgetBase* UWidgetModuleStatics::CreateUserWidgetByTag(FGameplayTag InWidgetTag, UObject* InOwner, const FParameter& InInitParameter, TSubclassOf<UUserWidgetBase> InClassOverride)
{
	return UWidgetModule::Get().CreateUserWidgetByTag(InWidgetTag, InOwner, &InInitParameter, InClassOverride);
}

bool UWidgetModuleStatics::OpenUserWidgetByTag(FGameplayTag InWidgetTag, const FParameter& InOpenParameter, bool bInstant, bool bForce)
{
	return UWidgetModule::Get().OpenUserWidgetByTag(InWidgetTag, InOpenParameter, bInstant, bForce);
}

bool UWidgetModuleStatics::CloseUserWidgetByTag(FGameplayTag InWidgetTag, bool bInstant)
{
	return UWidgetModule::Get().CloseUserWidgetByTag(InWidgetTag, bInstant);
}

bool UWidgetModuleStatics::ToggleUserWidgetByTag(FGameplayTag InWidgetTag, bool bInstant)
{
	return UWidgetModule::Get().ToggleUserWidgetByTag(InWidgetTag, bInstant);
}

bool UWidgetModuleStatics::DestroyUserWidgetByTag(FGameplayTag InWidgetTag, bool bRecovery)
{
	return UWidgetModule::Get().DestroyUserWidgetByTag(InWidgetTag, bRecovery);
}

bool UWidgetModuleStatics::HasUserWidgetClass(TSubclassOf<UUserWidgetBase> InClass)
{
	return UWidgetModule::Get().HasUserWidgetClass(InClass);
}

TSubclassOf<UUserWidgetBase> UWidgetModuleStatics::GetUserWidgetClass(TSubclassOf<UUserWidgetBase> InClass)
{
	return UWidgetModule::Get().GetUserWidgetClass(InClass);
}

bool UWidgetModuleStatics::HasUserWidget(TSubclassOf<UUserWidgetBase> InClass)
{
	return UWidgetModule::Get().HasUserWidget(InClass);
}

UUserWidgetBase* UWidgetModuleStatics::GetUserWidget(TSubclassOf<UUserWidgetBase> InClass)
{
	return GetDeterminesOutputObject(UWidgetModule::Get().GetUserWidget(InClass), InClass);
}

UUserWidgetBase* UWidgetModuleStatics::CreateUserWidget(TSubclassOf<UUserWidgetBase> InClass, UObject* InOwner, const TArray<FParameter>& InParams, bool bForce)
{
	return GetDeterminesOutputObject(
		UWidgetModule::Get().CreateUserWidget(InClass, InOwner, InParams, bForce),
		InClass);
}

bool UWidgetModuleStatics::OpenUserWidget(TSubclassOf<UUserWidgetBase> InClass, const TArray<FParameter>& InParams, bool bInstant, bool bForce)
{
	return UWidgetModule::Get().OpenUserWidget(InClass, InParams, bInstant, bForce);
}

bool UWidgetModuleStatics::CloseUserWidget(TSubclassOf<UUserWidgetBase> InClass, bool bInstant)
{
	return UWidgetModule::Get().CloseUserWidget(InClass, bInstant);
}

bool UWidgetModuleStatics::ToggleUserWidget(TSubclassOf<UUserWidgetBase> InClass, bool bInstant)
{
	return UWidgetModule::Get().ToggleUserWidget(InClass, bInstant);
}

bool UWidgetModuleStatics::DestroyUserWidget(TSubclassOf<UUserWidgetBase> InClass, bool bRecovery)
{
	return UWidgetModule::Get().DestroyUserWidget(InClass, bRecovery);
}

void UWidgetModuleStatics::CloseAllUserWidget(bool bInstant)
{
	UWidgetModule::Get().CloseAllUserWidget(bInstant);
}

void UWidgetModuleStatics::CloseAllSlateWidget(bool bInstant)
{
	FSlateWidgetManager::Get().CloseAllSlateWidget(bInstant);
}

UWorldWidgetContainer* UWidgetModuleStatics::GetWorldWidgetContainer()
{
	return UWidgetModule::Get().GetWorldWidgetContainer();
}

TArray<UWorldWidgetBase*> UWidgetModuleStatics::GetWorldWidgetsByTag(FGameplayTag InWidgetTag)
{
	return UWidgetModule::Get().GetWorldWidgetsByTag(InWidgetTag);
}

UWorldWidgetBase* UWidgetModuleStatics::GetWorldWidgetByTag(FGameplayTag InWidgetTag, int32 InIndex, TSubclassOf<UWorldWidgetBase> InExpectedClass)
{
	return GetDeterminesOutputObject(
		UWidgetModule::Get().GetWorldWidgetByTag(InWidgetTag, InIndex, InExpectedClass),
		InExpectedClass);
}

UWorldWidgetBase* UWidgetModuleStatics::CreateWorldWidgetByTag(FGameplayTag InWidgetTag, UObject* InOwner, FWorldWidgetMapping InMapping, const TArray<FParameter>& InParams, TSubclassOf<UWorldWidgetBase> InClassOverride)
{
	return UWidgetModule::Get().CreateWorldWidgetByTag(
		InWidgetTag,
		InOwner,
		InMapping,
		InParams,
		InClassOverride);
}

bool UWidgetModuleStatics::DestroyWorldWidgetByTag(FGameplayTag InWidgetTag, UWorldWidgetBase* InWidget, bool bRecovery)
{
	return UWidgetModule::Get().DestroyWorldWidgetByTag(InWidgetTag, InWidget, bRecovery);
}

bool UWidgetModuleStatics::GetWorldWidgetVisibleByTag(FGameplayTag InWidgetTag)
{
	return UWidgetModule::Get().GetWorldWidgetVisibleByTag(InWidgetTag);
}

void UWidgetModuleStatics::SetWorldWidgetVisibleByTag(FGameplayTag InWidgetTag, bool bVisible)
{
	UWidgetModule::Get().SetWorldWidgetVisibleByTag(InWidgetTag, bVisible);
}

bool UWidgetModuleStatics::GetWorldWidgetVisible(TSubclassOf<UWorldWidgetBase> InClass)
{
	return UWidgetModule::Get().GetWorldWidgetVisible(InClass);
}

void UWidgetModuleStatics::SetWorldWidgetVisible(bool bVisible, TSubclassOf<UWorldWidgetBase> InClass)
{
	UWidgetModule::Get().SetWorldWidgetVisible(bVisible, InClass);
}

bool UWidgetModuleStatics::HasWorldWidget(TSubclassOf<UWorldWidgetBase> InClass, int32 InIndex)
{
	return UWidgetModule::Get().HasWorldWidget(InClass, InIndex);
}

UWorldWidgetBase* UWidgetModuleStatics::GetWorldWidget(TSubclassOf<UWorldWidgetBase> InClass, int32 InIndex)
{
	return GetDeterminesOutputObject(
		UWidgetModule::Get().GetWorldWidget(InClass, InIndex),
		InClass);
}

TArray<UWorldWidgetBase*> UWidgetModuleStatics::GetWorldWidgets(TSubclassOf<UWorldWidgetBase> InClass)
{
	return UWidgetModule::Get().GetWorldWidgets(InClass);
}

UWorldWidgetBase* UWidgetModuleStatics::CreateWorldWidget(TSubclassOf<UWorldWidgetBase> InClass, UObject* InOwner, FWorldWidgetMapping InMapping, const TArray<FParameter>& InParams)
{
	return GetDeterminesOutputObject(
		UWidgetModule::Get().CreateWorldWidget(InClass, InOwner, InMapping, InParams),
		InClass);
}

bool UWidgetModuleStatics::DestroyWorldWidget(TSubclassOf<UWorldWidgetBase> InClass, int32 InIndex, bool bRecovery)
{
	return UWidgetModule::Get().DestroyWorldWidget(InClass, InIndex, bRecovery);
}

void UWidgetModuleStatics::DestroyWorldWidgets(TSubclassOf<UWorldWidgetBase> InClass, bool bRecovery)
{
	UWidgetModule::Get().DestroyWorldWidgets(InClass, bRecovery);
}
