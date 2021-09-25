// Fill out your copyright notice in the Description page of Project Settings.


#include "Widget/WidgetModuleBPLibrary.h"

#include "MainModule.h"
#include "MainModuleBPLibrary.h"
#include "WidgetModule.h"
#include "Kismet/GameplayStatics.h"

AWidgetModule* UWidgetModuleBPLibrary::WidgetModuleInst = nullptr;

AWidgetModule* UWidgetModuleBPLibrary::GetWidgetModule(UObject* InWorldContext)
{
	if (!WidgetModuleInst || !WidgetModuleInst->IsValidLowLevel())
	{
		if(AMainModule* MainModule = UMainModuleBPLibrary::GetMainModule(InWorldContext))
		{
			WidgetModuleInst = MainModule->GetModuleByClass<AWidgetModule>();
		}
	}
	return WidgetModuleInst;
}

UUserWidgetBase* UWidgetModuleBPLibrary::K2_CreateUserWidget(UObject* InWorldContext, TSubclassOf<UUserWidgetBase> InWidgetClass)
{
	if (AWidgetModule* WidgetModule = GetWidgetModule(InWorldContext))
	{
		return WidgetModule->K2_CreateUserWidget(InWidgetClass);
	}
	return nullptr;
}

UUserWidgetBase* UWidgetModuleBPLibrary::K2_GetUserWidget(UObject* InWorldContext, TSubclassOf<UUserWidgetBase> InWidgetClass)
{
	if (AWidgetModule* WidgetModule = GetWidgetModule(InWorldContext))
	{
		return WidgetModule->K2_GetUserWidget(InWidgetClass);
	}
	return nullptr;
}

bool UWidgetModuleBPLibrary::K2_InitializeUserWidget(UObject* InWorldContext, AActor* InOwner, TSubclassOf<UUserWidgetBase> InWidgetClass)
{
	if (AWidgetModule* WidgetModule = GetWidgetModule(InWorldContext))
	{
		return WidgetModule->K2_InitializeUserWidget(InOwner, InWidgetClass);
	}
	return false;
}

bool UWidgetModuleBPLibrary::K2_OpenUserWidget(UObject* InWorldContext, TSubclassOf<UUserWidgetBase> InWidgetClass, bool bInstant)
{
	if (AWidgetModule* WidgetModule = GetWidgetModule(InWorldContext))
	{
		return WidgetModule->K2_OpenUserWidget(InWidgetClass, bInstant);
	}
	return false;
}

bool UWidgetModuleBPLibrary::K2_CloseUserWidget(UObject* InWorldContext, TSubclassOf<UUserWidgetBase> InWidgetClass, bool bInstant)
{
	if (AWidgetModule* WidgetModule = GetWidgetModule(InWorldContext))
	{
		return WidgetModule->K2_CloseUserWidget(InWidgetClass, bInstant);
	}
	return false;
}

bool UWidgetModuleBPLibrary::K2_ToggleUserWidget(UObject* InWorldContext, TSubclassOf<UUserWidgetBase> InWidgetClass, bool bInstant)
{
	if (AWidgetModule* WidgetModule = GetWidgetModule(InWorldContext))
	{
		return WidgetModule->K2_ToggleUserWidget(InWidgetClass, bInstant);
	}
	return false;
}

bool UWidgetModuleBPLibrary::K2_DestroyUserWidget(UObject* InWorldContext, TSubclassOf<UUserWidgetBase> InWidgetClass)
{
	if (AWidgetModule* WidgetModule = GetWidgetModule(InWorldContext))
	{
		return WidgetModule->K2_DestroyUserWidget(InWidgetClass);
	}
	return false;
}

void UWidgetModuleBPLibrary::OpenAllUserWidget(UObject* InWorldContext, EWidgetType InWidgetType, bool bInstant)
{
	if (AWidgetModule* WidgetModule = GetWidgetModule(InWorldContext))
	{
		WidgetModule->OpenAllUserWidget(InWidgetType, bInstant);
	}
}

void UWidgetModuleBPLibrary::CloseAllUserWidget(UObject* InWorldContext, EWidgetType InWidgetType, bool bInstant)
{
	if (AWidgetModule* WidgetModule = GetWidgetModule(InWorldContext))
	{
		WidgetModule->CloseAllUserWidget(InWidgetType, bInstant);
	}
}

void UWidgetModuleBPLibrary::OpenAllSlateWidget(UObject* InWorldContext, EWidgetType InWidgetType, bool bInstant)
{
	if (AWidgetModule* WidgetModule = GetWidgetModule(InWorldContext))
	{
		WidgetModule->OpenAllSlateWidget(InWidgetType, bInstant);
	}
}

void UWidgetModuleBPLibrary::CloseAllSlateWidget(UObject* InWorldContext, EWidgetType InWidgetType, bool bInstant)
{
	if (AWidgetModule* WidgetModule = GetWidgetModule(InWorldContext))
	{
		WidgetModule->CloseAllSlateWidget(InWidgetType, bInstant);
	}
}

void UWidgetModuleBPLibrary::SetInputMode(UObject* InWorldContext, EInputMode InInputMode)
{
	if(AWidgetModule* WidgetModule = GetWidgetModule(InWorldContext))
	{
		WidgetModule->SetInputMode(InInputMode);
	}
}

EInputMode UWidgetModuleBPLibrary::GetInputMode(UObject* InWorldContext)
{
	if(AWidgetModule* WidgetModule = GetWidgetModule(InWorldContext))
	{
		return WidgetModule->GetInputMode();
	}
	return EInputMode::None;
}
