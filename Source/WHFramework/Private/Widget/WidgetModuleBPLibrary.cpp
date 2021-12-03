// Fill out your copyright notice in the Description page of Project Settings.


#include "Widget/WidgetModuleBPLibrary.h"

#include "Main/MainModule.h"
#include "Kismet/GameplayStatics.h"

UUserWidgetBase* UWidgetModuleBPLibrary::K2_CreateUserWidget(TSubclassOf<UUserWidgetBase> InWidgetClass)
{
	if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
	{
		return WidgetModule->K2_CreateUserWidget(InWidgetClass);
	}
	return nullptr;
}

UUserWidgetBase* UWidgetModuleBPLibrary::K2_GetUserWidget(TSubclassOf<UUserWidgetBase> InWidgetClass)
{
	if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
	{
		return WidgetModule->K2_GetUserWidget(InWidgetClass);
	}
	return nullptr;
}

bool UWidgetModuleBPLibrary::K2_InitializeUserWidget(AActor* InOwner, TSubclassOf<UUserWidgetBase> InWidgetClass)
{
	if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
	{
		return WidgetModule->K2_InitializeUserWidget(InOwner, InWidgetClass);
	}
	return false;
}

bool UWidgetModuleBPLibrary::K2_OpenUserWidget(TSubclassOf<UUserWidgetBase> InWidgetClass, bool bInstant)
{
	if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
	{
		return WidgetModule->K2_OpenUserWidget(InWidgetClass, bInstant);
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

void UWidgetModuleBPLibrary::OpenAllUserWidget(EWidgetType InWidgetType, bool bInstant)
{
	if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
	{
		WidgetModule->OpenAllUserWidget(InWidgetType, bInstant);
	}
}

void UWidgetModuleBPLibrary::CloseAllUserWidget(EWidgetType InWidgetType, bool bInstant)
{
	if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
	{
		WidgetModule->CloseAllUserWidget(InWidgetType, bInstant);
	}
}

void UWidgetModuleBPLibrary::OpenAllSlateWidget(EWidgetType InWidgetType, bool bInstant)
{
	if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
	{
		WidgetModule->OpenAllSlateWidget(InWidgetType, bInstant);
	}
}

void UWidgetModuleBPLibrary::CloseAllSlateWidget(EWidgetType InWidgetType, bool bInstant)
{
	if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
	{
		WidgetModule->CloseAllSlateWidget(InWidgetType, bInstant);
	}
}

void UWidgetModuleBPLibrary::SetInputMode(EInputMode InInputMode)
{
	if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
	{
		WidgetModule->SetInputMode(InInputMode);
	}
}

EInputMode UWidgetModuleBPLibrary::GetInputMode()
{
	if(AWidgetModule* WidgetModule = AMainModule::GetModuleByClass<AWidgetModule>())
	{
		return WidgetModule->GetInputMode();
	}
	return EInputMode::None;
}
