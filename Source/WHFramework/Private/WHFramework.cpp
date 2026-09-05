// Copyright Epic Games, Inc. All Rights Reserved.

#include "WHFramework.h"
#include "UObject/CoreRedirects.h"

#define LOCTEXT_NAMESPACE "FWHFrameworkModule"

void FWHFrameworkModule::StartupModule()
{
	TArray<FCoreRedirect> Redirects;
	const TPair<const TCHAR*, const TCHAR*> Classes[] = {
		{TEXT("Dialogue"), TEXT("DialogueAsset")},
		{TEXT("DialogueEvents"), TEXT("DialogueEventBase")},
		{TEXT("DialogueConditions"), TEXT("DialogueConditionBase")},
		{TEXT("OrCondition"), TEXT("DialogueCondition_Or")},
		{TEXT("AndCondition"), TEXT("DialogueCondition_And")},
		{TEXT("DialogueUserWidget"), TEXT("WidgetDialogueBase")}
	};
	for (const auto& Class : Classes)
	{
		const FString Destination = FString(TEXT("/Script/WHFramework.")) + Class.Value;
		Redirects.Emplace(ECoreRedirectFlags::Type_Class, FString(TEXT("/Script/DialoguePlugin.")) + Class.Key, Destination);
		Redirects.Emplace(ECoreRedirectFlags::Type_Class, FString(TEXT("/Script/WHFramework.")) + Class.Key, Destination);
	}
	Redirects.Emplace(ECoreRedirectFlags::Type_Struct, TEXT("/Script/DialoguePlugin.DialogueNode"), TEXT("/Script/WHFramework.DialogueNode"));
	Redirects.Emplace(ECoreRedirectFlags::Type_Package, TEXT("/DialoguePlugin/DialogueSoundConcurrency"), TEXT("/WHFramework/Dialogue/DialogueSoundConcurrency"));
	Redirects.Emplace(ECoreRedirectFlags::Type_Package, TEXT("/DialoguePlugin/UI/DemoDialogueWidget"), TEXT("/WHFramework/Dialogue/UI/DemoDialogueWidget"));
	Redirects.Emplace(ECoreRedirectFlags::Type_Package, TEXT("/DialoguePlugin/UI/DemoReplyWidget"), TEXT("/WHFramework/Dialogue/UI/DemoReplyWidget"));
	FCoreRedirects::AddRedirectList(Redirects, TEXT("WHFrameworkDialogueMigration"));
}

void FWHFrameworkModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FWHFrameworkModule, WHFramework)