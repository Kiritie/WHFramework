// Fill out your copyright notice in the Description page of Project Settings.


#include "Widget/WidgetModuleStatics.h"

UUserWidgetBase* UWidgetModuleStatics::GetTemporaryUserWidget()
{
	if(UWidgetModule* WidgetModule = UWidgetModule::Get())
	{
		return WidgetModule->GetTemporaryUserWidget();
	}
	return nullptr;
}

bool UWidgetModuleStatics::HasUserWidgetClass(TSubclassOf<UUserWidgetBase> InClass)
{
	if(UWidgetModule* WidgetModule = UWidgetModule::Get())
	{
		return WidgetModule->HasUserWidgetClass(InClass);
	}
	return false;
}

bool UWidgetModuleStatics::HasUserWidget(TSubclassOf<UUserWidgetBase> InClass)
{
	if(UWidgetModule* WidgetModule = UWidgetModule::Get())
	{
		return WidgetModule->HasUserWidget(InClass);
	}
	return false;
}

UUserWidgetBase* UWidgetModuleStatics::GetUserWidget(TSubclassOf<UUserWidgetBase> InClass)
{
	if(UWidgetModule* WidgetModule = UWidgetModule::Get())
	{
		return WidgetModule->GetUserWidget(InClass);
	}
	return nullptr;
}

UUserWidgetBase* UWidgetModuleStatics::GetUserWidgetByName(FName InName, TSubclassOf<UUserWidgetBase> InClass)
{
	if(UWidgetModule* WidgetModule = UWidgetModule::Get())
	{
		return WidgetModule->GetUserWidgetByName(InName, InClass);
	}
	return nullptr;
}

UUserWidgetBase* UWidgetModuleStatics::CreateUserWidget(TSubclassOf<UUserWidgetBase> InClass, UObject* InOwner)
{
	if(UWidgetModule* WidgetModule = UWidgetModule::Get())
	{
		return WidgetModule->CreateUserWidget(InClass, InOwner);
	}
	return nullptr;
}

UUserWidgetBase* UWidgetModuleStatics::CreateUserWidgetByName(FName InName, UObject* InOwner)
{
	if(UWidgetModule* WidgetModule = UWidgetModule::Get())
	{
		return WidgetModule->CreateUserWidgetByName(InName, InOwner);
	}
	return nullptr;
}

bool UWidgetModuleStatics::OpenUserWidget(TSubclassOf<UUserWidgetBase> InClass, const TArray<FParameter>& InParams, bool bInstant)
{
	if(UWidgetModule* WidgetModule = UWidgetModule::Get())
	{
		return WidgetModule->OpenUserWidget(InClass, InParams, bInstant);
	}
	return false;
}

bool UWidgetModuleStatics::OpenUserWidgetByName(FName InName, const TArray<FParameter>& InParams, bool bInstant)
{
	if(UWidgetModule* WidgetModule = UWidgetModule::Get())
	{
		return WidgetModule->OpenUserWidgetByName(InName, InParams, bInstant);
	}
	return false;
}

bool UWidgetModuleStatics::CloseUserWidget(TSubclassOf<UUserWidgetBase> InClass, bool bInstant)
{
	if(UWidgetModule* WidgetModule = UWidgetModule::Get())
	{
		return WidgetModule->CloseUserWidget(InClass, bInstant);
	}
	return false;
}

bool UWidgetModuleStatics::ToggleUserWidget(TSubclassOf<UUserWidgetBase> InClass, bool bInstant)
{
	if(UWidgetModule* WidgetModule = UWidgetModule::Get())
	{
		return WidgetModule->ToggleUserWidget(InClass, bInstant);
	}
	return false;
}

bool UWidgetModuleStatics::DestroyUserWidget(TSubclassOf<UUserWidgetBase> InClass, bool bRecovery)
{
	if(UWidgetModule* WidgetModule = UWidgetModule::Get())
	{
		return WidgetModule->DestroyUserWidget(InClass, bRecovery);
	}
	return false;
}

void UWidgetModuleStatics::CloseAllUserWidget(bool bInstant)
{
	if(UWidgetModule* WidgetModule = UWidgetModule::Get())
	{
		WidgetModule->CloseAllUserWidget(bInstant);
	}
}

void UWidgetModuleStatics::CloseAllSlateWidget(bool bInstant)
{
	if(UWidgetModule* WidgetModule = UWidgetModule::Get())
	{
		WidgetModule->CloseAllSlateWidget(bInstant);
	}
}

UWorldWidgetContainer* UWidgetModuleStatics::GetWorldWidgetContainer()
{
	if(UWidgetModule* WidgetModule = UWidgetModule::Get())
	{
		return WidgetModule->GetWorldWidgetContainer();
	}
	return nullptr;
}

bool UWidgetModuleStatics::HasWorldWidget(TSubclassOf<UWorldWidgetBase> InClass, int32 InIndex)
{
	if(UWidgetModule* WidgetModule = UWidgetModule::Get())
	{
		return WidgetModule->HasWorldWidget(InClass, InIndex);
	}
	return false;
}

UWorldWidgetBase* UWidgetModuleStatics::GetWorldWidget(TSubclassOf<UWorldWidgetBase> InClass, int32 InIndex)
{
	if(UWidgetModule* WidgetModule = UWidgetModule::Get())
	{
		return WidgetModule->GetWorldWidget(InClass, InIndex);
	}
	return nullptr;
}

UWorldWidgetBase* UWidgetModuleStatics::GetWorldWidgetByName(FName InName, TSubclassOf<UWorldWidgetBase> InClass, int32 InIndex)
{
	if(UWidgetModule* WidgetModule = UWidgetModule::Get())
	{
		return WidgetModule->GetWorldWidgetByName(InName, InClass, InIndex);
	}
	return nullptr;
}

TArray<UWorldWidgetBase*> UWidgetModuleStatics::GetWorldWidgets(TSubclassOf<UWorldWidgetBase> InClass)
{
	if(UWidgetModule* WidgetModule = UWidgetModule::Get())
	{
		return WidgetModule->GetWorldWidgets(InClass);
	}
	return TArray<UWorldWidgetBase*>();
}

TArray<UWorldWidgetBase*> UWidgetModuleStatics::GetWorldWidgetsByName(FName InName)
{
	if(UWidgetModule* WidgetModule = UWidgetModule::Get())
	{
		return WidgetModule->GetWorldWidgetsByName(InName);
	}
	return TArray<UWorldWidgetBase*>();
}

UWorldWidgetBase* UWidgetModuleStatics::CreateWorldWidget(TSubclassOf<UWorldWidgetBase> InClass, UObject* InOwner, FWorldWidgetBindInfo InBindInfo, const TArray<FParameter>& InParams)
{
	if(UWidgetModule* WidgetModule = UWidgetModule::Get())
	{
		return WidgetModule->CreateWorldWidget(InClass, InOwner, InBindInfo, InParams);
	}
	return nullptr;
}

UWorldWidgetBase* UWidgetModuleStatics::CreateWorldWidgetByName(FName InName, TSubclassOf<UWorldWidgetBase> InClass, UObject* InOwner, FWorldWidgetBindInfo InBindInfo, const TArray<FParameter>& InParams)
{
	if(UWidgetModule* WidgetModule = UWidgetModule::Get())
	{
		return WidgetModule->CreateWorldWidgetByName(InName, InClass, InOwner, InBindInfo, InParams);
	}
	return nullptr;
}

bool UWidgetModuleStatics::DestroyWorldWidget(TSubclassOf<UWorldWidgetBase> InClass, int32 InIndex, bool bRecovery)
{
	if(UWidgetModule* WidgetModule = UWidgetModule::Get())
	{
		return WidgetModule->DestroyWorldWidget(InClass, InIndex, bRecovery);
	}
	return false;
}

void UWidgetModuleStatics::DestroyWorldWidgets(TSubclassOf<UWorldWidgetBase> InClass, bool bRecovery)
{
	if(UWidgetModule* WidgetModule = UWidgetModule::Get())
	{
		WidgetModule->DestroyWorldWidgets(InClass, bRecovery);
	}
}
