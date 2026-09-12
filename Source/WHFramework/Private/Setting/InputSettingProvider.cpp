#include "Setting/InputSettingProvider.h"

#include "Input/InputModule.h"

void UInputSettingProvider::CollectDefinitions(TArray<FSettingDefinition>& OutDefinitions) const
{
	for(const FGameplayTag& ActionTag : UInputModule::Get().GetAllMappableActions())
	{
		const TArray<FPlayerKeyMapping> Mappings = UInputModule::Get().GetPlayerKeyMappingsByTag(ActionTag);
		for(const FPlayerKeyMapping& Mapping : Mappings)
		{
			FString ActionName = ActionTag.ToString();
			ActionName.RemoveFromStart(TEXT("Input."));

			FSettingDefinition& Definition = OutDefinitions.AddDefaulted_GetRef();
			Definition.SettingId.Name = FName(*FString::Printf(TEXT("Input.%s.%d"), *ActionName, static_cast<int32>(Mapping.GetSlot())));
			Definition.Page = FName(TEXT("Input"));
			Definition.Category = FName(TEXT("Bindings"));
			Definition.Order = static_cast<int32>(Mapping.GetSlot());
			Definition.DisplayName = FText::FromString(ActionName);
			Definition.Renderer = ESettingRendererType::Key;
			Definition.ApplyPolicy = ESettingApplyPolicy::Deferred;
			Definition.SemanticTag = ActionTag;
		}
	}
}

bool UInputSettingProvider::CanHandle(const FSettingDefinition& InDefinition) const
{
	return InDefinition.Renderer == ESettingRendererType::Key && InDefinition.SemanticTag.IsValid();
}

FParameter UInputSettingProvider::GetAppliedValue(const FSettingDefinition& InDefinition) const
{
	const TArray<FPlayerKeyMapping> Mappings = UInputModule::Get().GetPlayerKeyMappingsByTag(InDefinition.SemanticTag);
	for(const FPlayerKeyMapping& Mapping : Mappings)
	{
		if(static_cast<int32>(Mapping.GetSlot()) == InDefinition.Order)
		{
			return FParameter(Mapping.GetCurrentKey());
		}
	}
	return FParameter();
}

FParameter UInputSettingProvider::GetPendingValue(const FSettingDefinition& InDefinition) const
{
	if(const FKey* Key = PendingKeys.Find(InDefinition.SettingId))
	{
		return FParameter(*Key);
	}
	return GetAppliedValue(InDefinition);
}

bool UInputSettingProvider::SetPendingValue(const FSettingDefinition& InDefinition, const FParameter& InValue)
{
	FKey Key;
	if(!InValue.TryGet(Key))
	{
		return false;
	}
	PendingKeys.Add(InDefinition.SettingId, Key);
	return true;
}

bool UInputSettingProvider::Apply(const FSettingDefinition& InDefinition, const FParameter& InValue)
{
	FKey Key;
	if(!InValue.TryGet(Key))
	{
		return false;
	}
	const bool bResult = UInputModule::Get().MapPlayerKeyByTag(InDefinition.SemanticTag, Key, static_cast<EPlayerMappableKeySlot>(InDefinition.Order));
	if(bResult)
	{
		PendingKeys.Remove(InDefinition.SettingId);
	}
	return bResult;
}
