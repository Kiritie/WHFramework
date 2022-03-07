// Fill out your copyright notice in the Description page of Project Settings.


#include "Widget/WidgetModuleBPLibrary.h"

#include "Main/MainModule.h"
#include "Kismet/GameplayStatics.h"

UUserWidgetBase* UWidgetModuleBPLibrary::K2_GetUserWidget(TSubclassOf<UUserWidgetBase> InWidgetClass)
{
	if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
	{
		return WidgetModule->K2_GetUserWidget(InWidgetClass);
	}
	return nullptr;
}

UUserWidgetBase* UWidgetModuleBPLibrary::K2_GetUserWidgetByName(FName InWidgetName, TSubclassOf<UUserWidgetBase> InWidgetClass)
{
	if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
	{
		return WidgetModule->K2_GetUserWidgetByName(InWidgetName, InWidgetClass);
	}
	return nullptr;
}

UUserWidgetBase* UWidgetModuleBPLibrary::K2_CreateUserWidget(TSubclassOf<UUserWidgetBase> InWidgetClass, AActor* InOwner)
{
	if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
	{
		return WidgetModule->K2_CreateUserWidget(InWidgetClass, InOwner);
	}
	return nullptr;
}

UUserWidgetBase* UWidgetModuleBPLibrary::K2_CreateUserWidgetByName(FName InWidgetName, AActor* InOwner)
{
	if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
	{
		return WidgetModule->K2_CreateUserWidgetByName(InWidgetName, InOwner);
	}
	return nullptr;
}

bool UWidgetModuleBPLibrary::K2_InitializeUserWidget(TSubclassOf<UUserWidgetBase> InWidgetClass, AActor* InOwner)
{
	if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
	{
		return WidgetModule->K2_InitializeUserWidget(InWidgetClass, InOwner);
	}
	return false;
}

bool UWidgetModuleBPLibrary::K2_OpenUserWidget(TSubclassOf<UUserWidgetBase> InWidgetClass, const TArray<FParameter>& InParams, bool bInstant)
{
	if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
	{
		return WidgetModule->K2_OpenUserWidget(InWidgetClass, InParams, bInstant);
	}
	return false;
}

bool UWidgetModuleBPLibrary::K2_CloseUserWidget(TSubclassOf<UUserWidgetBase> InWidgetClass, bool bInstant)
{
	if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
	{
		return WidgetModule->K2_CloseUserWidget(InWidgetClass, bInstant);
	}
	return false;
}

bool UWidgetModuleBPLibrary::K2_ToggleUserWidget(TSubclassOf<UUserWidgetBase> InWidgetClass, bool bInstant)
{
	if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
	{
		return WidgetModule->K2_ToggleUserWidget(InWidgetClass, bInstant);
	}
	return false;
}

bool UWidgetModuleBPLibrary::K2_DestroyUserWidget(TSubclassOf<UUserWidgetBase> InWidgetClass)
{
	if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
	{
		return WidgetModule->K2_DestroyUserWidget(InWidgetClass);
	}
	return false;
}

void UWidgetModuleBPLibrary::CloseAllUserWidget(EWidgetType InWidgetType, bool bInstant)
{
	if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
	{
		WidgetModule->CloseAllUserWidget(InWidgetType, bInstant);
	}
}

void UWidgetModuleBPLibrary::CloseAllSlateWidget(EWidgetType InWidgetType, bool bInstant)
{
	if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
	{
		WidgetModule->CloseAllSlateWidget(InWidgetType, bInstant);
	}
}

bool UWidgetModuleBPLibrary::K2_HasWorldWidget(TSubclassOf<UWorldWidgetBase> InWidgetClass, int32 InWidgetIndex)
{
	if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
	{
		return WidgetModule->K2_HasWorldWidget(InWidgetClass, InWidgetIndex);
	}
	return false;
}

UWorldWidgetBase* UWidgetModuleBPLibrary::K2_GetWorldWidget(TSubclassOf<UWorldWidgetBase> InWidgetClass, int32 InWidgetIndex)
{
	if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
	{
		return WidgetModule->K2_GetWorldWidget(InWidgetClass, InWidgetIndex);
	}
	return nullptr;
}

UWorldWidgetBase* UWidgetModuleBPLibrary::K2_GetWorldWidgetByName(FName InWidgetName, TSubclassOf<UWorldWidgetBase> InWidgetClass, int32 InWidgetIndex)
{
	if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
	{
		return WidgetModule->K2_GetWorldWidgetByName(InWidgetName, InWidgetClass, InWidgetIndex);
	}
	return nullptr;
}

TArray<UWorldWidgetBase*> UWidgetModuleBPLibrary::K2_GetWorldWidgets(TSubclassOf<UWorldWidgetBase> InWidgetClass)
{
	if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
	{
		return WidgetModule->K2_GetWorldWidgets(InWidgetClass);
	}
	return TArray<UWorldWidgetBase*>();
}

TArray<UWorldWidgetBase*> UWidgetModuleBPLibrary::K2_GetWorldWidgetsByName(FName InWidgetName)
{
	if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
	{
		return WidgetModule->K2_GetWorldWidgetsByName(InWidgetName);
	}
	return TArray<UWorldWidgetBase*>();
}

UWorldWidgetBase* UWidgetModuleBPLibrary::K2_CreateWorldWidget(TSubclassOf<UWorldWidgetBase> InWidgetClass, AActor* InOwner, FVector InLocation, USceneComponent* InSceneComp, const TArray<FParameter>& InParams)
{
	if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
	{
		return WidgetModule->K2_CreateWorldWidget(InWidgetClass, InOwner, InLocation, InSceneComp, InParams);
	}
	return nullptr;
}

bool UWidgetModuleBPLibrary::K2_DestroyWorldWidget(TSubclassOf<UWorldWidgetBase> InWidgetClass, int32 InWidgetIndex)
{
	if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
	{
		return WidgetModule->K2_DestroyWorldWidget(InWidgetClass, InWidgetIndex);
	}
	return false;
}

void UWidgetModuleBPLibrary::K2_DestroyWorldWidgets(TSubclassOf<UWorldWidgetBase> InWidgetClass)
{
	if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
	{
		WidgetModule->K2_DestroyWorldWidgets(InWidgetClass);
	}
}
