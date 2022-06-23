// Fill out your copyright notice in the Description page of Project Settings.


#include "Widget/WidgetModuleBPLibrary.h"

#include "Main/MainModule.h"
#include "Kismet/GameplayStatics.h"

UUserWidgetBase* UWidgetModuleBPLibrary::GetMainUserWidget()
{
	if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
	{
		return WidgetModule->GetMainUserWidget();
	}
	return nullptr;
}

UUserWidgetBase* UWidgetModuleBPLibrary::GetTemporaryUserWidget()
{
	if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
	{
		return WidgetModule->GetTemporaryUserWidget();
	}
	return nullptr;
}

bool UWidgetModuleBPLibrary::HasUserWidgetClass(TSubclassOf<UUserWidgetBase> InWidgetClass)
{
	if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
	{
		return WidgetModule->HasUserWidgetClass(InWidgetClass);
	}
	return false;
}

bool UWidgetModuleBPLibrary::HasUserWidget(TSubclassOf<UUserWidgetBase> InWidgetClass)
{
	if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
	{
		return WidgetModule->HasUserWidget(InWidgetClass);
	}
	return false;
}

UUserWidgetBase* UWidgetModuleBPLibrary::GetUserWidget(TSubclassOf<UUserWidgetBase> InWidgetClass)
{
	if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
	{
		return WidgetModule->GetUserWidget(InWidgetClass);
	}
	return nullptr;
}

UUserWidgetBase* UWidgetModuleBPLibrary::GetUserWidgetByName(FName InWidgetName, TSubclassOf<UUserWidgetBase> InWidgetClass)
{
	if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
	{
		return WidgetModule->GetUserWidgetByName(InWidgetName, InWidgetClass);
	}
	return nullptr;
}

UUserWidgetBase* UWidgetModuleBPLibrary::CreateUserWidget(TSubclassOf<UUserWidgetBase> InWidgetClass, AActor* InOwner)
{
	if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
	{
		return WidgetModule->CreateUserWidget(InWidgetClass, InOwner);
	}
	return nullptr;
}

UUserWidgetBase* UWidgetModuleBPLibrary::CreateUserWidgetByName(FName InWidgetName, AActor* InOwner)
{
	if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
	{
		return WidgetModule->CreateUserWidgetByName(InWidgetName, InOwner);
	}
	return nullptr;
}

bool UWidgetModuleBPLibrary::InitializeUserWidget(TSubclassOf<UUserWidgetBase> InWidgetClass, AActor* InOwner)
{
	if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
	{
		return WidgetModule->InitializeUserWidget(InWidgetClass, InOwner);
	}
	return false;
}

bool UWidgetModuleBPLibrary::OpenUserWidget(TSubclassOf<UUserWidgetBase> InWidgetClass, const TArray<FParameter>& InParams, bool bInstant)
{
	if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
	{
		return WidgetModule->OpenUserWidget(InWidgetClass, InParams, bInstant);
	}
	return false;
}

bool UWidgetModuleBPLibrary::OpenUserWidgetByName(FName InWidgetName, const TArray<FParameter>& InParams, bool bInstant)
{
	if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
	{
		return WidgetModule->OpenUserWidgetByName(InWidgetName, InParams, bInstant);
	}
	return false;
}

bool UWidgetModuleBPLibrary::CloseUserWidget(TSubclassOf<UUserWidgetBase> InWidgetClass, bool bInstant)
{
	if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
	{
		return WidgetModule->CloseUserWidget(InWidgetClass, bInstant);
	}
	return false;
}

bool UWidgetModuleBPLibrary::ToggleUserWidget(TSubclassOf<UUserWidgetBase> InWidgetClass, bool bInstant)
{
	if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
	{
		return WidgetModule->ToggleUserWidget(InWidgetClass, bInstant);
	}
	return false;
}

bool UWidgetModuleBPLibrary::DestroyUserWidget(TSubclassOf<UUserWidgetBase> InWidgetClass, bool bRecovery)
{
	if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
	{
		return WidgetModule->DestroyUserWidget(InWidgetClass, bRecovery);
	}
	return false;
}

void UWidgetModuleBPLibrary::CloseAllUserWidget(bool bInstant)
{
	if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
	{
		WidgetModule->CloseAllUserWidget(bInstant);
	}
}

void UWidgetModuleBPLibrary::CloseAllSlateWidget(bool bInstant)
{
	if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
	{
		WidgetModule->CloseAllSlateWidget(bInstant);
	}
}

UWorldWidgetContainer* UWidgetModuleBPLibrary::GetWorldWidgetContainer()
{
	if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
	{
		return WidgetModule->GetWorldWidgetContainer();
	}
	return nullptr;
}

bool UWidgetModuleBPLibrary::HasWorldWidget(TSubclassOf<UWorldWidgetBase> InWidgetClass, int32 InWidgetIndex)
{
	if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
	{
		return WidgetModule->HasWorldWidget(InWidgetClass, InWidgetIndex);
	}
	return false;
}

UWorldWidgetBase* UWidgetModuleBPLibrary::GetWorldWidget(TSubclassOf<UWorldWidgetBase> InWidgetClass, int32 InWidgetIndex)
{
	if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
	{
		return WidgetModule->GetWorldWidget(InWidgetClass, InWidgetIndex);
	}
	return nullptr;
}

UWorldWidgetBase* UWidgetModuleBPLibrary::GetWorldWidgetByName(FName InWidgetName, TSubclassOf<UWorldWidgetBase> InWidgetClass, int32 InWidgetIndex)
{
	if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
	{
		return WidgetModule->GetWorldWidgetByName(InWidgetName, InWidgetClass, InWidgetIndex);
	}
	return nullptr;
}

TArray<UWorldWidgetBase*> UWidgetModuleBPLibrary::GetWorldWidgets(TSubclassOf<UWorldWidgetBase> InWidgetClass)
{
	if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
	{
		return WidgetModule->GetWorldWidgets(InWidgetClass);
	}
	return TArray<UWorldWidgetBase*>();
}

TArray<UWorldWidgetBase*> UWidgetModuleBPLibrary::GetWorldWidgetsByName(FName InWidgetName)
{
	if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
	{
		return WidgetModule->GetWorldWidgetsByName(InWidgetName);
	}
	return TArray<UWorldWidgetBase*>();
}

UWorldWidgetBase* UWidgetModuleBPLibrary::CreateWorldWidget(TSubclassOf<UWorldWidgetBase> InWidgetClass, AActor* InOwner, FVector InLocation, USceneComponent* InSceneComp, const TArray<FParameter>& InParams)
{
	if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
	{
		return WidgetModule->CreateWorldWidget(InWidgetClass, InOwner, InLocation, InSceneComp, InParams);
	}
	return nullptr;
}

UWorldWidgetBase* UWidgetModuleBPLibrary::CreateWorldWidgetByName(FName InWidgetName, TSubclassOf<UWorldWidgetBase> InWidgetClass, AActor* InOwner, FVector InLocation, USceneComponent* InSceneComp, const TArray<FParameter>& InParams)
{
	if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
	{
		return WidgetModule->CreateWorldWidgetByName(InWidgetName, InWidgetClass, InOwner, InLocation, InSceneComp, InParams);
	}
	return nullptr;
}

bool UWidgetModuleBPLibrary::DestroyWorldWidget(TSubclassOf<UWorldWidgetBase> InWidgetClass, int32 InWidgetIndex, bool bRecovery)
{
	if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
	{
		return WidgetModule->DestroyWorldWidget(InWidgetClass, InWidgetIndex, bRecovery);
	}
	return false;
}

void UWidgetModuleBPLibrary::DestroyWorldWidgets(TSubclassOf<UWorldWidgetBase> InWidgetClass, bool bRecovery)
{
	if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
	{
		WidgetModule->DestroyWorldWidgets(InWidgetClass, bRecovery);
	}
}
