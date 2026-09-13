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

UUserWidgetBase* UWidgetModuleStatics::CreateUserWidgetByTag(FGameplayTag InWidgetTag, const FParameter& InParam, TSubclassOf<UUserWidgetBase> InClass)
{
	return UWidgetModule::Get().CreateUserWidgetByTag(InWidgetTag, InParam, InClass);
}

bool UWidgetModuleStatics::OpenUserWidgetByTag(FGameplayTag InWidgetTag, const FParameter& InParam, bool bInstant, bool bForce)
{
	return UWidgetModule::Get().OpenUserWidgetByTag(InWidgetTag, InParam, bInstant, bForce);
}

bool UWidgetModuleStatics::CloseUserWidgetByTag(FGameplayTag InWidgetTag, bool bInstant)
{
	return UWidgetModule::Get().CloseUserWidgetByTag(InWidgetTag, bInstant);
}

bool UWidgetModuleStatics::ToggleUserWidgetByTag(FGameplayTag InWidgetTag, bool bInstant)
{
	return UWidgetModule::Get().ToggleUserWidgetByTag(InWidgetTag, bInstant);
}

bool UWidgetModuleStatics::DestroyUserWidgetByTag(FGameplayTag InWidgetTag, EObjectDespawnMode InMode)
{
	return UWidgetModule::Get().DestroyUserWidgetByTag(InWidgetTag, InMode);
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

UUserWidgetBase* UWidgetModuleStatics::CreateUserWidget(TSubclassOf<UUserWidgetBase> InClass, const FParameter& InParam)
{
	return GetDeterminesOutputObject(
		UWidgetModule::Get().CreateUserWidget(InClass, InParam),
		InClass);
}

bool UWidgetModuleStatics::OpenUserWidget(TSubclassOf<UUserWidgetBase> InClass, const FParameter& InParam, bool bInstant, bool bForce)
{
	return UWidgetModule::Get().OpenUserWidget(InClass, InParam, bInstant, bForce);
}

bool UWidgetModuleStatics::CloseUserWidget(TSubclassOf<UUserWidgetBase> InClass, bool bInstant)
{
	return UWidgetModule::Get().CloseUserWidget(InClass, bInstant);
}

bool UWidgetModuleStatics::ToggleUserWidget(TSubclassOf<UUserWidgetBase> InClass, bool bInstant)
{
	return UWidgetModule::Get().ToggleUserWidget(InClass, bInstant);
}

bool UWidgetModuleStatics::DestroyUserWidget(TSubclassOf<UUserWidgetBase> InClass, EObjectDespawnMode InMode)
{
	return UWidgetModule::Get().DestroyUserWidget(InClass, InMode);
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

UWorldWidgetBase* UWidgetModuleStatics::CreateWorldWidgetByTag(FGameplayTag InWidgetTag, FWorldWidgetMapping InMapping, const FParameter& InParam, TSubclassOf<UWorldWidgetBase> InClass)
{
	return UWidgetModule::Get().CreateWorldWidgetByTag(
		InWidgetTag,
		InMapping,
		InParam,
		InClass);
}

bool UWidgetModuleStatics::DestroyWorldWidgetByTag(FGameplayTag InWidgetTag, UWorldWidgetBase* InWidget, EObjectDespawnMode InMode)
{
	return UWidgetModule::Get().DestroyWorldWidgetByTag(InWidgetTag, InWidget, InMode);
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

TArray<UWorldWidgetBase*> UWidgetModuleStatics::GetWorldWidgets(TSubclassOf<UWorldWidgetBase> InClass)
{
	return UWidgetModule::Get().GetWorldWidgets(InClass);
}

UWorldWidgetBase* UWidgetModuleStatics::CreateWorldWidget(TSubclassOf<UWorldWidgetBase> InClass, FWorldWidgetMapping InMapping, const FParameter& InParam)
{
	return GetDeterminesOutputObject(
		UWidgetModule::Get().CreateWorldWidget(InClass, InMapping, InParam),
		InClass);
}

void UWidgetModuleStatics::DestroyWorldWidgets(TSubclassOf<UWorldWidgetBase> InClass, EObjectDespawnMode InMode)
{
	UWidgetModule::Get().DestroyWorldWidgets(InClass, InMode);
}
