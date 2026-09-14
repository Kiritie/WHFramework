#include "Setting/Provider/ParameterSettingProvider.h"

#include "Parameter/ParameterModule.h"

namespace
{
	const FName ParameterProviderName(TEXT("Parameter"));

	FName MakeParameterSettingId(const FParameterSet& InSet)
	{
		const FString Category = InSet.Category.IsEmpty() ? TEXT("General") : InSet.Category.ToString();
		return FName(*FString::Printf(TEXT("Parameter.%s.%s"), *Category, *InSet.Name.ToString()));
	}

	ESettingRendererType InferParameterRenderer(const FParameter& InValue)
	{
		if(InValue.Is<bool>())
		{
			return ESettingRendererType::Bool;
		}
		if(InValue.Is<int32>() || InValue.Is<int64>() || InValue.Is<float>() || InValue.Is<double>())
		{
			return ESettingRendererType::Number;
		}
		if(InValue.Is<FString>() || InValue.Is<FName>() || InValue.Is<FText>())
		{
			return ESettingRendererType::Text;
		}
		return ESettingRendererType::Custom;
	}
}

void UParameterSettingProvider::CollectDefinitions(TArray<FSettingDefinition>& OutDefinitions) const
{
	int32 Order = 0;
	for(const FParameterSet& ParameterSet : UParameterModule::Get().GetAllParameter())
	{
		FSettingDefinition& Definition = OutDefinitions.AddDefaulted_GetRef();
		Definition.SettingId = FSettingId(MakeParameterSettingId(ParameterSet));
		Definition.Provider = ParameterProviderName;
		Definition.Page = ParameterProviderName;
		Definition.Category = ParameterSet.Category.IsEmpty() ? FName(TEXT("General")) : FName(*ParameterSet.Category.ToString());
		Definition.Order = Order++;
		Definition.DisplayName = FText::FromName(ParameterSet.Name);
		Definition.Renderer = InferParameterRenderer(ParameterSet.Parameter);
		Definition.ApplyPolicy = ESettingApplyPolicy::Deferred;
	}
}

bool UParameterSettingProvider::CanHandle(const FSettingDefinition& InDefinition) const
{
	return InDefinition.Provider == ParameterProviderName;
}

void UParameterSettingProvider::BeginEdit(const TArray<FSettingDefinition>& InDefinitions)
{
	Super::BeginEdit(InDefinitions);
	AppliedValues.Reset();
	PendingValues.Reset();
	DefaultValues.Reset();
	ParameterNames.Reset();

	for(const FParameterSet& ParameterSet : UParameterModule::Get().GetAllParameter())
	{
		const FSettingId SettingId(MakeParameterSettingId(ParameterSet));
		AppliedValues.Add(SettingId, ParameterSet.Parameter);
		PendingValues.Add(SettingId, ParameterSet.Parameter);
		DefaultValues.Add(SettingId, ParameterSet.Parameter);
		ParameterNames.Add(SettingId, ParameterSet.Name);
	}
}

FParameter UParameterSettingProvider::GetAppliedValue(const FSettingDefinition& InDefinition) const
{
	const FParameter* Value = AppliedValues.Find(InDefinition.SettingId);
	return Value ? *Value : FParameter();
}

FParameter UParameterSettingProvider::GetPendingValue(const FSettingDefinition& InDefinition) const
{
	const FParameter* Value = PendingValues.Find(InDefinition.SettingId);
	return Value ? *Value : FParameter();
}

FParameter UParameterSettingProvider::GetDefaultValue(const FSettingDefinition& InDefinition) const
{
	const FParameter* Value = DefaultValues.Find(InDefinition.SettingId);
	return Value ? *Value : FParameter();
}

bool UParameterSettingProvider::SetPendingValue(const FSettingDefinition& InDefinition, const FParameter& InValue)
{
	if(!CanHandle(InDefinition) || !ParameterNames.Contains(InDefinition.SettingId))
	{
		return false;
	}
	PendingValues.Add(InDefinition.SettingId, InValue);
	return true;
}

bool UParameterSettingProvider::Apply(const FSettingDefinition& InDefinition, const FParameter& InValue)
{
	const FName* ParameterName = ParameterNames.Find(InDefinition.SettingId);
	if(!ParameterName)
	{
		return false;
	}
	UParameterModule::Get().SetParameter(*ParameterName, InValue);
	return true;
}

void UParameterSettingProvider::Rollback(const FSettingDefinition& InDefinition)
{
	if(const FParameter* Value = AppliedValues.Find(InDefinition.SettingId))
	{
		Apply(InDefinition, *Value);
		PendingValues.Add(InDefinition.SettingId, *Value);
	}
}

void UParameterSettingProvider::Commit()
{
	AppliedValues = PendingValues;
}

void UParameterSettingProvider::EndEdit()
{
	AppliedValues.Reset();
	PendingValues.Reset();
	DefaultValues.Reset();
	ParameterNames.Reset();
}
