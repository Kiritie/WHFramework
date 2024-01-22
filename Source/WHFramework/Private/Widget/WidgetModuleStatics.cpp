// Fill out your copyright notice in the Description page of Project Settings.


#include "Widget/WidgetModuleStatics.h"

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
	UWidgetModule::Get().CloseAllSlateWidget(bInstant);
}

UWorldWidgetContainer* UWidgetModuleStatics::GetWorldWidgetContainer()
{
	return UWidgetModule::Get().GetWorldWidgetContainer();
}

bool UWidgetModuleStatics::GetWorldWidgetVisible(TSubclassOf<UWorldWidgetBase> InClass)
{
	return UWidgetModule::Get().GetWorldWidgetVisible(InClass);
}

void UWidgetModuleStatics::SetWorldWidgetVisible(bool bVisible)
{
	UWidgetModule::Get().SetWorldWidgetVisible(bVisible);
}

bool UWidgetModuleStatics::HasWorldWidget(TSubclassOf<UWorldWidgetBase> InClass, int32 InIndex)
{
	return UWidgetModule::Get().HasWorldWidget(InClass, InIndex);
}

UWorldWidgetBase* UWidgetModuleStatics::GetWorldWidget(TSubclassOf<UWorldWidgetBase> InClass, int32 InIndex)
{
	return UWidgetModule::Get().GetWorldWidget(InClass, InIndex);
}

UWorldWidgetBase* UWidgetModuleStatics::GetWorldWidgetByName(FName InName, TSubclassOf<UWorldWidgetBase> InClass, int32 InIndex)
{
	return UWidgetModule::Get().GetWorldWidgetByName(InName, InClass, InIndex);
}

TArray<UWorldWidgetBase*> UWidgetModuleStatics::GetWorldWidgets(TSubclassOf<UWorldWidgetBase> InClass)
{
	return UWidgetModule::Get().GetWorldWidgets(InClass);
}

TArray<UWorldWidgetBase*> UWidgetModuleStatics::GetWorldWidgetsByName(FName InName)
{
	return UWidgetModule::Get().GetWorldWidgetsByName(InName);
}

UWorldWidgetBase* UWidgetModuleStatics::CreateWorldWidget(TSubclassOf<UWorldWidgetBase> InClass, UObject* InOwner, FWorldWidgetMapping InMapping, const TArray<FParameter>& InParams)
{
	return UWidgetModule::Get().CreateWorldWidget(InClass, InOwner, InMapping, InParams);
}

UWorldWidgetBase* UWidgetModuleStatics::CreateWorldWidgetByName(FName InName, TSubclassOf<UWorldWidgetBase> InClass, UObject* InOwner, FWorldWidgetMapping InMapping, const TArray<FParameter>& InParams)
{
	return UWidgetModule::Get().CreateWorldWidgetByName(InName, InClass, InOwner, InMapping, InParams);
}

bool UWidgetModuleStatics::DestroyWorldWidget(TSubclassOf<UWorldWidgetBase> InClass, int32 InIndex, bool bRecovery)
{
	return UWidgetModule::Get().DestroyWorldWidget(InClass, InIndex, bRecovery);
}

void UWidgetModuleStatics::DestroyWorldWidgets(TSubclassOf<UWorldWidgetBase> InClass, bool bRecovery)
{
	UWidgetModule::Get().DestroyWorldWidgets(InClass, bRecovery);
}
