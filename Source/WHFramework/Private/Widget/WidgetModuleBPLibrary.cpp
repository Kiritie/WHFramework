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

bool UWidgetModuleBPLibrary::HasUserWidgetClass(TSubclassOf<UUserWidgetBase> InClass)
{
	if(AWidgetModule* WidgetModule = AWidgetModule::Get())
	{
		return WidgetModule->HasUserWidgetClass(InClass);
	}
	return false;
}

bool UWidgetModuleBPLibrary::HasUserWidget(TSubclassOf<UUserWidgetBase> InClass)
{
	if(AWidgetModule* WidgetModule = AWidgetModule::Get())
	{
		return WidgetModule->HasUserWidget(InClass);
	}
	return false;
}

UUserWidgetBase* UWidgetModuleBPLibrary::GetUserWidget(TSubclassOf<UUserWidgetBase> InClass)
{
	if(AWidgetModule* WidgetModule = AWidgetModule::Get())
	{
		return WidgetModule->GetUserWidget(InClass);
	}
	return nullptr;
}

UUserWidgetBase* UWidgetModuleBPLibrary::GetUserWidgetByName(FName InName, TSubclassOf<UUserWidgetBase> InClass)
{
	if(AWidgetModule* WidgetModule = AWidgetModule::Get())
	{
		return WidgetModule->GetUserWidgetByName(InName, InClass);
	}
	return nullptr;
}

UUserWidgetBase* UWidgetModuleBPLibrary::CreateUserWidget(TSubclassOf<UUserWidgetBase> InClass, AActor* InOwner)
{
	if(AWidgetModule* WidgetModule = AWidgetModule::Get())
	{
		return WidgetModule->CreateUserWidget(InClass, InOwner);
	}
	return nullptr;
}

UUserWidgetBase* UWidgetModuleBPLibrary::CreateUserWidgetByName(FName InName, AActor* InOwner)
{
	if(AWidgetModule* WidgetModule = AWidgetModule::Get())
	{
		return WidgetModule->CreateUserWidgetByName(InName, InOwner);
	}
	return nullptr;
}

bool UWidgetModuleBPLibrary::InitializeUserWidget(TSubclassOf<UUserWidgetBase> InClass, AActor* InOwner)
{
	if(AWidgetModule* WidgetModule = AWidgetModule::Get())
	{
		return WidgetModule->InitializeUserWidget(InClass, InOwner);
	}
	return false;
}

bool UWidgetModuleBPLibrary::OpenUserWidget(TSubclassOf<UUserWidgetBase> InClass, const TArray<FParameter>& InParams, bool bInstant)
{
	if(AWidgetModule* WidgetModule = AWidgetModule::Get())
	{
		return WidgetModule->OpenUserWidget(InClass, InParams, bInstant);
	}
	return false;
}

bool UWidgetModuleBPLibrary::OpenUserWidgetByName(FName InName, const TArray<FParameter>& InParams, bool bInstant)
{
	if(AWidgetModule* WidgetModule = AWidgetModule::Get())
	{
		return WidgetModule->OpenUserWidgetByName(InName, InParams, bInstant);
	}
	return false;
}

bool UWidgetModuleBPLibrary::CloseUserWidget(TSubclassOf<UUserWidgetBase> InClass, bool bInstant)
{
	if(AWidgetModule* WidgetModule = AWidgetModule::Get())
	{
		return WidgetModule->CloseUserWidget(InClass, bInstant);
	}
	return false;
}

bool UWidgetModuleBPLibrary::ToggleUserWidget(TSubclassOf<UUserWidgetBase> InClass, bool bInstant)
{
	if(AWidgetModule* WidgetModule = AWidgetModule::Get())
	{
		return WidgetModule->ToggleUserWidget(InClass, bInstant);
	}
	return false;
}

bool UWidgetModuleBPLibrary::DestroyUserWidget(TSubclassOf<UUserWidgetBase> InClass, bool bRecovery)
{
	if(AWidgetModule* WidgetModule = AWidgetModule::Get())
	{
		return WidgetModule->DestroyUserWidget(InClass, bRecovery);
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

bool UWidgetModuleBPLibrary::HasWorldWidget(TSubclassOf<UWorldWidgetBase> InClass, int32 InIndex)
{
	if(AWidgetModule* WidgetModule = AWidgetModule::Get())
	{
		return WidgetModule->HasWorldWidget(InClass, InIndex);
	}
	return false;
}

UWorldWidgetBase* UWidgetModuleBPLibrary::GetWorldWidget(TSubclassOf<UWorldWidgetBase> InClass, int32 InIndex)
{
	if(AWidgetModule* WidgetModule = AWidgetModule::Get())
	{
		return WidgetModule->GetWorldWidget(InClass, InIndex);
	}
	return nullptr;
}

UWorldWidgetBase* UWidgetModuleBPLibrary::GetWorldWidgetByName(FName InName, TSubclassOf<UWorldWidgetBase> InClass, int32 InIndex)
{
	if(AWidgetModule* WidgetModule = AWidgetModule::Get())
	{
		return WidgetModule->GetWorldWidgetByName(InName, InClass, InIndex);
	}
	return nullptr;
}

TArray<UWorldWidgetBase*> UWidgetModuleBPLibrary::GetWorldWidgets(TSubclassOf<UWorldWidgetBase> InClass)
{
	if(AWidgetModule* WidgetModule = AWidgetModule::Get())
	{
		return WidgetModule->GetWorldWidgets(InClass);
	}
	return TArray<UWorldWidgetBase*>();
}

TArray<UWorldWidgetBase*> UWidgetModuleBPLibrary::GetWorldWidgetsByName(FName InName)
{
	if(AWidgetModule* WidgetModule = AWidgetModule::Get())
	{
		return WidgetModule->GetWorldWidgetsByName(InName);
	}
	return TArray<UWorldWidgetBase*>();
}

UWorldWidgetBase* UWidgetModuleBPLibrary::CreateWorldWidget(TSubclassOf<UWorldWidgetBase> InClass, AActor* InOwner, FVector InLocation, USceneComponent* InSceneComp, const TArray<FParameter>& InParams)
{
	if(AWidgetModule* WidgetModule = AWidgetModule::Get())
	{
		return WidgetModule->CreateWorldWidget(InClass, InOwner, InLocation, InSceneComp, InParams);
	}
	return nullptr;
}

UWorldWidgetBase* UWidgetModuleBPLibrary::CreateWorldWidgetByName(FName InName, TSubclassOf<UWorldWidgetBase> InClass, AActor* InOwner, FVector InLocation, USceneComponent* InSceneComp, const TArray<FParameter>& InParams)
{
	if(AWidgetModule* WidgetModule = AWidgetModule::Get())
	{
		return WidgetModule->CreateWorldWidgetByName(InName, InClass, InOwner, InLocation, InSceneComp, InParams);
	}
	return nullptr;
}

bool UWidgetModuleBPLibrary::DestroyWorldWidget(TSubclassOf<UWorldWidgetBase> InClass, int32 InIndex, bool bRecovery)
{
	if(AWidgetModule* WidgetModule = AWidgetModule::Get())
	{
		return WidgetModule->DestroyWorldWidget(InClass, InIndex, bRecovery);
	}
	return false;
}

void UWidgetModuleBPLibrary::DestroyWorldWidgets(TSubclassOf<UWorldWidgetBase> InClass, bool bRecovery)
{
	if(AWidgetModule* WidgetModule = AWidgetModule::Get())
	{
		WidgetModule->DestroyWorldWidgets(InClass, bRecovery);
	}
}
