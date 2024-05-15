// Fill out your copyright notice in the Description page of Project Settings.


#include "Widget/WidgetModuleStatics.h"

#include "Widget/Common/CommonToolTip.h"

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
	return UObjectPoolModuleStatics::SpawnObject<UCommonToolTip>(InWidget, nullptr, false, InToolTipClass);
}

void UWidgetModuleStatics::DestroyToolTipForWidget(UWidget* InWidget)
{
	UObjectPoolModuleStatics::DespawnObject(InWidget->GetToolTip());
}

UUserWidgetBase* UWidgetModuleStatics::GetTemporaryUserWidget()
{
	return UWidgetModule::Get().GetTemporaryUserWidget();
}

bool UWidgetModuleStatics::HasUserWidgetClass(TSubclassOf<UUserWidgetBase> InClass)
{
	return UWidgetModule::Get().HasUserWidgetClass(InClass);
}

void UWidgetModuleStatics::AddUserWidgetClassMapping(FName InName, TSubclassOf<UUserWidgetBase> InClass)
{
	return UWidgetModule::Get().AddUserWidgetClassMapping(InName, InClass);
}

bool UWidgetModuleStatics::HasUserWidget(TSubclassOf<UUserWidgetBase> InClass)
{
	return UWidgetModule::Get().HasUserWidget(InClass);
}

UUserWidgetBase* UWidgetModuleStatics::GetUserWidget(TSubclassOf<UUserWidgetBase> InClass)
{
	return GetDeterminesOutputObject(UWidgetModule::Get().GetUserWidget(InClass), InClass);
}

UUserWidgetBase* UWidgetModuleStatics::GetUserWidgetByName(FName InName, TSubclassOf<UUserWidgetBase> InClass)
{
	return GetDeterminesOutputObject(UWidgetModule::Get().GetUserWidgetByName(InName, InClass), InClass);
}

UUserWidgetBase* UWidgetModuleStatics::CreateUserWidget(TSubclassOf<UUserWidgetBase> InClass, UObject* InOwner, const TArray<FParameter>& InParams, bool bForce)
{
	return GetDeterminesOutputObject(UWidgetModule::Get().CreateUserWidget(InClass, InOwner, InParams, bForce), InClass);
}

UUserWidgetBase* UWidgetModuleStatics::CreateUserWidgetByName(FName InName, UObject* InOwner, const TArray<FParameter>& InParams, bool bForce)
{
	return UWidgetModule::Get().CreateUserWidgetByName(InName, InOwner, InParams, bForce);
}

bool UWidgetModuleStatics::OpenUserWidget(TSubclassOf<UUserWidgetBase> InClass, const TArray<FParameter>& InParams, bool bInstant, bool bForce)
{
	return UWidgetModule::Get().OpenUserWidget(InClass, InParams, bInstant, bForce);
}

bool UWidgetModuleStatics::OpenUserWidgetByName(FName InName, const TArray<FParameter>& InParams, bool bInstant, bool bForce)
{
	return UWidgetModule::Get().OpenUserWidgetByName(InName, InParams, bInstant, bForce);
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

bool UWidgetModuleStatics::GetWorldWidgetVisible(bool bInEditor, TSubclassOf<UWorldWidgetBase> InClass)
{
	return UWidgetModule::Get(bInEditor).GetWorldWidgetVisible(InClass);
}

void UWidgetModuleStatics::SetWorldWidgetVisible(bool bVisible, bool bInEditor, TSubclassOf<UWorldWidgetBase> InClass)
{
	UWidgetModule::Get(bInEditor).SetWorldWidgetVisible(bVisible, InClass);
}

bool UWidgetModuleStatics::HasWorldWidget(TSubclassOf<UWorldWidgetBase> InClass, int32 InIndex, bool bInEditor)
{
	return UWidgetModule::Get(bInEditor).HasWorldWidget(InClass, InIndex);
}

UWorldWidgetBase* UWidgetModuleStatics::GetWorldWidget(TSubclassOf<UWorldWidgetBase> InClass, int32 InIndex, bool bInEditor)
{
	return UWidgetModule::Get(bInEditor).GetWorldWidget(InClass, InIndex);
}

UWorldWidgetBase* UWidgetModuleStatics::GetWorldWidgetByName(FName InName, TSubclassOf<UWorldWidgetBase> InClass, int32 InIndex, bool bInEditor)
{
	return UWidgetModule::Get(bInEditor).GetWorldWidgetByName(InName, InClass, InIndex);
}

TArray<UWorldWidgetBase*> UWidgetModuleStatics::GetWorldWidgets(TSubclassOf<UWorldWidgetBase> InClass, bool bInEditor)
{
	return UWidgetModule::Get(bInEditor).GetWorldWidgets(InClass);
}

TArray<UWorldWidgetBase*> UWidgetModuleStatics::GetWorldWidgetsByName(FName InName, bool bInEditor)
{
	return UWidgetModule::Get(bInEditor).GetWorldWidgetsByName(InName);
}

UWorldWidgetBase* UWidgetModuleStatics::CreateWorldWidget(TSubclassOf<UWorldWidgetBase> InClass, UObject* InOwner, FWorldWidgetMapping InMapping, const TArray<FParameter>& InParams, bool bInEditor)
{
	return UWidgetModule::Get(bInEditor).CreateWorldWidget(InClass, InOwner, InMapping, InParams);
}

UWorldWidgetBase* UWidgetModuleStatics::CreateWorldWidgetByName(FName InName, UObject* InOwner, FWorldWidgetMapping InMapping, const TArray<FParameter>& InParams, bool bInEditor)
{
	return UWidgetModule::Get(bInEditor).CreateWorldWidgetByName(InName, InOwner, InMapping, InParams);
}

bool UWidgetModuleStatics::DestroyWorldWidget(TSubclassOf<UWorldWidgetBase> InClass, int32 InIndex, bool bRecovery, bool bInEditor)
{
	return UWidgetModule::Get(bInEditor).DestroyWorldWidget(InClass, InIndex, bRecovery);
}

void UWidgetModuleStatics::DestroyWorldWidgets(TSubclassOf<UWorldWidgetBase> InClass, bool bRecovery, bool bInEditor)
{
	UWidgetModule::Get(bInEditor).DestroyWorldWidgets(InClass, bRecovery);
}
