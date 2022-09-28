// Fill out your copyright notice in the Description page of Project Settings.


#include "Widget/WidgetModuleBPLibrary.h"

#include "Main/MainModule.h"
#include "Kismet/GameplayStatics.h"

UUserWidgetBase* UWidgetModuleBPLibrary::GetTemporaryUserWidget()
{
	if(AWidgetModule* WidgetModule = AWidgetModule::Get())
	{
		return WidgetModule->GetTemporaryUserWidget();
	}
	return nullptr;
}

bool UWidgetModuleBPLibrary::HasUserWidgetClass(TSubclassOf<UUserWidgetBase> InWidgetClass)
{
	if(AWidgetModule* WidgetModule = AWidgetModule::Get())
	{
		return WidgetModule->HasUserWidgetClass(InWidgetClass);
	}
	return false;
}

bool UWidgetModuleBPLibrary::HasUserWidget(TSubclassOf<UUserWidgetBase> InWidgetClass)
{
	if(AWidgetModule* WidgetModule = AWidgetModule::Get())
	{
		return WidgetModule->HasUserWidget(InWidgetClass);
	}
	return false;
}

UUserWidgetBase* UWidgetModuleBPLibrary::GetUserWidget(TSubclassOf<UUserWidgetBase> InWidgetClass)
{
	if(AWidgetModule* WidgetModule = AWidgetModule::Get())
	{
		return WidgetModule->GetUserWidget(InWidgetClass);
	}
	return nullptr;
}

UUserWidgetBase* UWidgetModuleBPLibrary::GetUserWidgetByName(FName InWidgetName, TSubclassOf<UUserWidgetBase> InWidgetClass)
{
	if(AWidgetModule* WidgetModule = AWidgetModule::Get())
	{
		return WidgetModule->GetUserWidgetByName(InWidgetName, InWidgetClass);
	}
	return nullptr;
}

UUserWidgetBase* UWidgetModuleBPLibrary::CreateUserWidget(TSubclassOf<UUserWidgetBase> InWidgetClass, AActor* InOwner)
{
	if(AWidgetModule* WidgetModule = AWidgetModule::Get())
	{
		return WidgetModule->CreateUserWidget(InWidgetClass, InOwner);
	}
	return nullptr;
}

UUserWidgetBase* UWidgetModuleBPLibrary::CreateUserWidgetByName(FName InWidgetName, AActor* InOwner)
{
	if(AWidgetModule* WidgetModule = AWidgetModule::Get())
	{
		return WidgetModule->CreateUserWidgetByName(InWidgetName, InOwner);
	}
	return nullptr;
}

bool UWidgetModuleBPLibrary::InitializeUserWidget(TSubclassOf<UUserWidgetBase> InWidgetClass, AActor* InOwner)
{
	if(AWidgetModule* WidgetModule = AWidgetModule::Get())
	{
		return WidgetModule->InitializeUserWidget(InWidgetClass, InOwner);
	}
	return false;
}

bool UWidgetModuleBPLibrary::OpenUserWidget(TSubclassOf<UUserWidgetBase> InWidgetClass, const TArray<FParameter>& InParams, bool bInstant)
{
	if(AWidgetModule* WidgetModule = AWidgetModule::Get())
	{
		return WidgetModule->OpenUserWidget(InWidgetClass, InParams, bInstant);
	}
	return false;
}

bool UWidgetModuleBPLibrary::OpenUserWidgetByName(FName InWidgetName, const TArray<FParameter>& InParams, bool bInstant)
{
	if(AWidgetModule* WidgetModule = AWidgetModule::Get())
	{
		return WidgetModule->OpenUserWidgetByName(InWidgetName, InParams, bInstant);
	}
	return false;
}

bool UWidgetModuleBPLibrary::CloseUserWidget(TSubclassOf<UUserWidgetBase> InWidgetClass, bool bInstant)
{
	if(AWidgetModule* WidgetModule = AWidgetModule::Get())
	{
		return WidgetModule->CloseUserWidget(InWidgetClass, bInstant);
	}
	return false;
}

bool UWidgetModuleBPLibrary::ToggleUserWidget(TSubclassOf<UUserWidgetBase> InWidgetClass, bool bInstant)
{
	if(AWidgetModule* WidgetModule = AWidgetModule::Get())
	{
		return WidgetModule->ToggleUserWidget(InWidgetClass, bInstant);
	}
	return false;
}

bool UWidgetModuleBPLibrary::DestroyUserWidget(TSubclassOf<UUserWidgetBase> InWidgetClass, bool bRecovery)
{
	if(AWidgetModule* WidgetModule = AWidgetModule::Get())
	{
		return WidgetModule->DestroyUserWidget(InWidgetClass, bRecovery);
	}
	return false;
}

void UWidgetModuleBPLibrary::CloseAllUserWidget(bool bInstant)
{
	if(AWidgetModule* WidgetModule = AWidgetModule::Get())
	{
		WidgetModule->CloseAllUserWidget(bInstant);
	}
}

void UWidgetModuleBPLibrary::CloseAllSlateWidget(bool bInstant)
{
	if(AWidgetModule* WidgetModule = AWidgetModule::Get())
	{
		WidgetModule->CloseAllSlateWidget(bInstant);
	}
}

UWorldWidgetContainer* UWidgetModuleBPLibrary::GetWorldWidgetContainer()
{
	if(AWidgetModule* WidgetModule = AWidgetModule::Get())
	{
		return WidgetModule->GetWorldWidgetContainer();
	}
	return nullptr;
}

bool UWidgetModuleBPLibrary::HasWorldWidget(TSubclassOf<UWorldWidgetBase> InWidgetClass, int32 InWidgetIndex)
{
	if(AWidgetModule* WidgetModule = AWidgetModule::Get())
	{
		return WidgetModule->HasWorldWidget(InWidgetClass, InWidgetIndex);
	}
	return false;
}

UWorldWidgetBase* UWidgetModuleBPLibrary::GetWorldWidget(TSubclassOf<UWorldWidgetBase> InWidgetClass, int32 InWidgetIndex)
{
	if(AWidgetModule* WidgetModule = AWidgetModule::Get())
	{
		return WidgetModule->GetWorldWidget(InWidgetClass, InWidgetIndex);
	}
	return nullptr;
}

UWorldWidgetBase* UWidgetModuleBPLibrary::GetWorldWidgetByName(FName InWidgetName, TSubclassOf<UWorldWidgetBase> InWidgetClass, int32 InWidgetIndex)
{
	if(AWidgetModule* WidgetModule = AWidgetModule::Get())
	{
		return WidgetModule->GetWorldWidgetByName(InWidgetName, InWidgetClass, InWidgetIndex);
	}
	return nullptr;
}

TArray<UWorldWidgetBase*> UWidgetModuleBPLibrary::GetWorldWidgets(TSubclassOf<UWorldWidgetBase> InWidgetClass)
{
	if(AWidgetModule* WidgetModule = AWidgetModule::Get())
	{
		return WidgetModule->GetWorldWidgets(InWidgetClass);
	}
	return TArray<UWorldWidgetBase*>();
}

TArray<UWorldWidgetBase*> UWidgetModuleBPLibrary::GetWorldWidgetsByName(FName InWidgetName)
{
	if(AWidgetModule* WidgetModule = AWidgetModule::Get())
	{
		return WidgetModule->GetWorldWidgetsByName(InWidgetName);
	}
	return TArray<UWorldWidgetBase*>();
}

UWorldWidgetBase* UWidgetModuleBPLibrary::CreateWorldWidget(TSubclassOf<UWorldWidgetBase> InWidgetClass, AActor* InOwner, FVector InLocation, USceneComponent* InSceneComp, const TArray<FParameter>& InParams)
{
	if(AWidgetModule* WidgetModule = AWidgetModule::Get())
	{
		return WidgetModule->CreateWorldWidget(InWidgetClass, InOwner, InLocation, InSceneComp, InParams);
	}
	return nullptr;
}

UWorldWidgetBase* UWidgetModuleBPLibrary::CreateWorldWidgetByName(FName InWidgetName, TSubclassOf<UWorldWidgetBase> InWidgetClass, AActor* InOwner, FVector InLocation, USceneComponent* InSceneComp, const TArray<FParameter>& InParams)
{
	if(AWidgetModule* WidgetModule = AWidgetModule::Get())
	{
		return WidgetModule->CreateWorldWidgetByName(InWidgetName, InWidgetClass, InOwner, InLocation, InSceneComp, InParams);
	}
	return nullptr;
}

bool UWidgetModuleBPLibrary::DestroyWorldWidget(TSubclassOf<UWorldWidgetBase> InWidgetClass, int32 InWidgetIndex, bool bRecovery)
{
	if(AWidgetModule* WidgetModule = AWidgetModule::Get())
	{
		return WidgetModule->DestroyWorldWidget(InWidgetClass, InWidgetIndex, bRecovery);
	}
	return false;
}

void UWidgetModuleBPLibrary::DestroyWorldWidgets(TSubclassOf<UWorldWidgetBase> InWidgetClass, bool bRecovery)
{
	if(AWidgetModule* WidgetModule = AWidgetModule::Get())
	{
		WidgetModule->DestroyWorldWidgets(InWidgetClass, bRecovery);
	}
}
