// Fill out your copyright notice in the Description page of Project Settings.

#include "Setting/SettingModule.h"

#include "Audio/AudioModule.h"
#include "Camera/CameraModule.h"
#include "Input/InputModule.h"
#include "Parameter/ParameterModule.h"
#include "SaveGame/Module/SettingSaveGame.h"
#include "Setting/SettingModuleNetworkComponent.h"
#include "Setting/SettingModuleTypes.h"
#include "Setting/SettingEntry.h"
#include "Setting/Provider/InputSettingProvider.h"
#include "Setting/Provider/LanguageSettingProvider.h"
#include "Setting/Provider/ResolutionSettingProvider.h"
#include "Setting/Provider/SettingProviderBase.h"
#include "Setting/SettingRegistry.h"
#include "Video/VideoModule.h"
#include "Widget/WidgetModule.h"
#include "UObject/UnrealType.h"

IMPLEMENTATION_MODULE(USettingModule)

namespace
{
	template<typename TProvider>
	void EnsureSettingProvider(
		UObject* InOuter,
		TArray<TObjectPtr<USettingProviderBase>>& InProviders)
	{
		if(InProviders.ContainsByPredicate([](const TObjectPtr<USettingProviderBase>& Provider)
		{
			return Provider && Provider->IsA<TProvider>();
		}))
		{
			return;
		}

		InProviders.Add(NewObject<TProvider>(InOuter, NAME_None, RF_Transactional));
	}

	bool TryGetNumericValue(const FParameter& InValue, double& OutValue)
	{
		if(const int32* Value = InValue.GetPtr<int32>())
		{
			OutValue = *Value;
			return true;
		}
		if(const int64* Value = InValue.GetPtr<int64>())
		{
			OutValue = static_cast<double>(*Value);
			return true;
		}
		if(const float* Value = InValue.GetPtr<float>())
		{
			OutValue = *Value;
			return true;
		}
		if(const double* Value = InValue.GetPtr<double>())
		{
			OutValue = *Value;
			return true;
		}
		return false;
	}
}

// Sets default values
USettingModule::USettingModule()
{
	ModuleName = FName("SettingModule");
	ModuleDisplayName = FText::FromString(TEXT("Setting Module"));

	ModuleSaveGame = USettingSaveGame::StaticClass();
	
	ModuleNetworkComponent = USettingModuleNetworkComponent::StaticClass();

	Providers.Add(CreateDefaultSubobject<UInputSettingProvider>(TEXT("InputSettingProvider")));
	Providers.Add(CreateDefaultSubobject<UResolutionSettingProvider>(TEXT("ResolutionSettingProvider")));
	Providers.Add(CreateDefaultSubobject<ULanguageSettingProvider>(TEXT("LanguageSettingProvider")));
}

USettingModule::~USettingModule()
{
	TERMINATION_MODULE(USettingModule)
}

#if WITH_EDITOR
void USettingModule::OnGenerate()
{
	Super::OnGenerate();
	BuildSettingDefinitions(true);
}

void USettingModule::OnDestroy()
{
	Super::OnDestroy();
}
#endif

void USettingModule::OnInitialize()
{
	Super::OnInitialize();
	BuildSettingDefinitions();
}

void USettingModule::OnPreparatory(EPhase InPhase)
{
	Super::OnPreparatory(InPhase);
}

void USettingModule::OnRefresh(float DeltaSeconds, bool bInEditor)
{
	Super::OnRefresh(DeltaSeconds, bInEditor);

	if(ConfirmationTransaction.bActive && FPlatformTime::Seconds() >= ConfirmationTransaction.ExpiresAt)
	{
		RejectPendingSettings();
	}
}

void USettingModule::OnPause()
{
	Super::OnPause();
}

void USettingModule::OnUnPause()
{
	Super::OnUnPause();
}

void USettingModule::OnTermination(EPhase InPhase)
{
	Super::OnTermination(InPhase);
	if(PHASEC(InPhase, EPhase::Primary))
	{
		EditSession = FSettingEditSession();
		ConfirmationTransaction = FSettingConfirmationTransaction();
		SettingEntries.Reset();
		SettingEntryMap.Reset();
		ValidationResults.Reset();
		for(USettingProviderBase* Provider : Providers)
		{
			if(Provider)
			{
				Provider->EndEdit();
			}
		}
	}
}

void USettingModule::CollectPropertyDefinitions(const UStruct* InStruct, const FString& InPrefix, TArray<FSettingDefinition>& OutDefinitions) const
{
	for(TFieldIterator<FProperty> Iterator(InStruct); Iterator; ++Iterator)
	{
		const FProperty* Property = *Iterator;
		if(Property->GetOwnerStruct() != InStruct || Property->HasMetaData(TEXT("SettingHidden")))
		{
			continue;
		}

		const FString PropertyPath = InPrefix.IsEmpty() ? Property->GetName() : InPrefix + TEXT(".") + Property->GetName();
		if(const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
		{
			if(StructProperty->Struct != TBaseStructure<FKey>::Get() && !Property->HasMetaData(TEXT("SettingLeaf")))
			{
				CollectPropertyDefinitions(StructProperty->Struct, PropertyPath, OutDefinitions);
				continue;
			}
		}

		if(!IsSupportedSettingProperty(Property))
		{
			continue;
		}

		FString SettingPath = PropertyPath;
		FString Root;
		FString Remainder;
		if(SettingPath.Split(TEXT("."), &Root, &Remainder) && Root.EndsWith(TEXT("Data")))
		{
			Root.LeftChopInline(4);
			SettingPath = Root + TEXT(".") + Remainder;
		}

		FSettingDefinition& Definition = OutDefinitions.AddDefaulted_GetRef();
		Definition.SettingId.Name = FName(*SettingPath);
		Definition.SourcePath = PropertyPath;
		Definition.Page = Property->HasMetaData(TEXT("SettingPage"))
			? FName(*Property->GetMetaData(TEXT("SettingPage")))
			: FName(*Root);
		if(Property->HasMetaData(TEXT("SettingCategory")))
		{
			Definition.Category = FName(*Property->GetMetaData(TEXT("SettingCategory")));
		}
		else if(Property->HasMetaData(TEXT("Category")))
		{
			Definition.Category = FName(*Property->GetMetaData(TEXT("Category")));
		}
		else
		{
			Definition.Category = FName(TEXT("General"));
		}
		Definition.Order = Property->HasMetaData(TEXT("SettingOrder")) ? FCString::Atoi(*Property->GetMetaData(TEXT("SettingOrder"))) : OutDefinitions.Num() - 1;
		Definition.DisplayName = Property->GetDisplayNameText();
		Definition.Description = Property->GetToolTipText();
		Definition.Renderer = InferRenderer(Property);
		const FString Renderer = Property->GetMetaData(TEXT("SettingRenderer"));
		if(Renderer.Equals(TEXT("Bool"), ESearchCase::IgnoreCase)) Definition.Renderer = ESettingRendererType::Bool;
		else if(Renderer.Equals(TEXT("Number"), ESearchCase::IgnoreCase)) Definition.Renderer = ESettingRendererType::Number;
		else if(Renderer.Equals(TEXT("Enum"), ESearchCase::IgnoreCase)) Definition.Renderer = ESettingRendererType::Enum;
		else if(Renderer.Equals(TEXT("Text"), ESearchCase::IgnoreCase)) Definition.Renderer = ESettingRendererType::Text;
		else if(Renderer.Equals(TEXT("Option"), ESearchCase::IgnoreCase)) Definition.Renderer = ESettingRendererType::Option;
		else if(Renderer.Equals(TEXT("Key"), ESearchCase::IgnoreCase)) Definition.Renderer = ESettingRendererType::Key;
		else if(Renderer.Equals(TEXT("Custom"), ESearchCase::IgnoreCase)) Definition.Renderer = ESettingRendererType::Custom;

		const FString ApplyPolicy = Property->GetMetaData(TEXT("SettingApply"));
		Definition.ApplyPolicy = ApplyPolicy.Equals(TEXT("Preview"), ESearchCase::IgnoreCase)
			? ESettingApplyPolicy::Preview
			: ApplyPolicy.Equals(TEXT("Immediate"), ESearchCase::IgnoreCase)
				? ESettingApplyPolicy::Immediate
				: ESettingApplyPolicy::Deferred;
		Definition.bVisible = !Property->HasMetaData(TEXT("SettingVisible")) || !Property->GetMetaData(TEXT("SettingVisible")).Equals(TEXT("false"), ESearchCase::IgnoreCase);
		Definition.bEnabled = !Property->HasMetaData(TEXT("SettingEnabled")) || !Property->GetMetaData(TEXT("SettingEnabled")).Equals(TEXT("false"), ESearchCase::IgnoreCase);
		Definition.bRequiresConfirmation = Property->HasMetaData(TEXT("SettingRequiresConfirmation"));

		const FString MinMetadata = Property->HasMetaData(TEXT("UIMin")) ? TEXT("UIMin") : TEXT("ClampMin");
		const FString MaxMetadata = Property->HasMetaData(TEXT("UIMax")) ? TEXT("UIMax") : TEXT("ClampMax");
		if(Property->HasMetaData(*MinMetadata))
		{
			Definition.NumberDisplay.bHasMin = true;
			Definition.NumberDisplay.Min = FCString::Atod(*Property->GetMetaData(*MinMetadata));
		}
		if(Property->HasMetaData(*MaxMetadata))
		{
			Definition.NumberDisplay.bHasMax = true;
			Definition.NumberDisplay.Max = FCString::Atod(*Property->GetMetaData(*MaxMetadata));
		}
		if(Property->HasMetaData(TEXT("Delta")))
		{
			Definition.NumberDisplay.Step = FCString::Atod(*Property->GetMetaData(TEXT("Delta")));
		}
		if(Property->HasMetaData(TEXT("SettingScale")))
		{
			Definition.NumberDisplay.Scale = FCString::Atod(*Property->GetMetaData(TEXT("SettingScale")));
		}
		if(Property->HasMetaData(TEXT("SettingDecimalPlaces")))
		{
			Definition.NumberDisplay.DecimalPlaces = FCString::Atoi(*Property->GetMetaData(TEXT("SettingDecimalPlaces")));
		}
		if(Property->HasMetaData(TEXT("Units")))
		{
			Definition.NumberDisplay.Suffix = FText::FromString(Property->GetMetaData(TEXT("Units")));
		}

		if(const FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
		{
			Definition.Enum = EnumProperty->GetEnum();
		}
		else if(const FByteProperty* ByteProperty = CastField<FByteProperty>(Property))
		{
			Definition.Enum = ByteProperty->Enum;
		}
		if(Definition.Enum)
		{
			for(int32 Index = 0; Index < Definition.Enum->NumEnums(); ++Index)
			{
				if(Definition.Enum->HasMetaData(TEXT("Hidden"), Index))
				{
					continue;
				}
				const int64 EnumValue = Definition.Enum->GetValueByIndex(Index);
				if(EnumValue == INDEX_NONE || Definition.Enum->GetNameStringByIndex(Index).EndsWith(TEXT("_MAX")))
				{
					continue;
				}

				FSettingOption& Option = Definition.Options.AddDefaulted_GetRef();
				Option.Value = FParameter(EnumValue);
				Option.DisplayName = Definition.Enum->GetDisplayNameTextByIndex(Index);
			}
		}
	}
}

bool USettingModule::IsSupportedSettingProperty(const FProperty* InProperty) const
{
	if(CastField<FBoolProperty>(InProperty) || CastField<FIntProperty>(InProperty) || CastField<FInt64Property>(InProperty) || CastField<FFloatProperty>(InProperty) || CastField<FDoubleProperty>(InProperty) || CastField<FNameProperty>(InProperty) || CastField<FStrProperty>(InProperty) || CastField<FTextProperty>(InProperty) || CastField<FEnumProperty>(InProperty))
	{
		return true;
	}
	if(const FByteProperty* ByteProperty = CastField<FByteProperty>(InProperty))
	{
		return ByteProperty->Enum != nullptr;
	}
	if(const FStructProperty* StructProperty = CastField<FStructProperty>(InProperty))
	{
		return StructProperty->Struct == TBaseStructure<FKey>::Get() || StructProperty->Struct == TBaseStructure<FIntPoint>::Get();
	}
	return false;
}

ESettingRendererType USettingModule::InferRenderer(const FProperty* InProperty) const
{
	if(CastField<FBoolProperty>(InProperty))
	{
		return ESettingRendererType::Bool;
	}
	if(CastField<FEnumProperty>(InProperty) || CastField<FByteProperty>(InProperty))
	{
		return ESettingRendererType::Enum;
	}
	if(CastField<FIntProperty>(InProperty) || CastField<FInt64Property>(InProperty) || CastField<FFloatProperty>(InProperty) || CastField<FDoubleProperty>(InProperty))
	{
		return ESettingRendererType::Number;
	}
	if(const FStructProperty* StructProperty = CastField<FStructProperty>(InProperty); StructProperty && StructProperty->Struct == TBaseStructure<FKey>::Get())
	{
		return ESettingRendererType::Key;
	}
	return ESettingRendererType::Text;
}

void USettingModule::EnsureBuiltinProviders()
{
	EnsureSettingProvider<UInputSettingProvider>(this, Providers);
	EnsureSettingProvider<UResolutionSettingProvider>(this, Providers);
	EnsureSettingProvider<ULanguageSettingProvider>(this, Providers);
}

void USettingModule::BuildSettingDefinitions(bool bUpdateSnapshot)
{
	EnsureBuiltinProviders();

	FinalDefinitions.Reset();
	if(!bUpdateSnapshot && Registry && !Registry->FinalDefinitions.IsEmpty())
	{
		FinalDefinitions = Registry->FinalDefinitions;
	}
	else
	{
		CollectPropertyDefinitions(FSettingModuleSaveData::StaticStruct(), FString(), FinalDefinitions);
	}
	for(const USettingProviderBase* Provider : Providers)
	{
		if(Provider)
		{
			Provider->CollectDefinitions(FinalDefinitions);
		}
	}

	if(Registry)
	{
		for(FSettingDefinition& Definition : FinalDefinitions)
		{
			const FSettingDefinitionOverride* Override = Registry->Overrides.FindByPredicate([&Definition](const FSettingDefinitionOverride& Item)
			{
				return Item.SettingId == Definition.SettingId;
			});
			if(!Override)
			{
				continue;
			}
			if(Override->bOverrideDisplayName) Definition.DisplayName = Override->DisplayName;
			if(Override->bOverrideDescription) Definition.Description = Override->Description;
			if(Override->bOverridePage) Definition.Page = Override->Page;
			if(Override->bOverrideCategory) Definition.Category = Override->Category;
			if(Override->bOverrideOrder) Definition.Order = Override->Order;
			if(Override->bOverrideRenderer) Definition.Renderer = Override->Renderer;
			if(Override->bOverrideApplyPolicy) Definition.ApplyPolicy = Override->ApplyPolicy;
			if(Override->bOverrideVisible) Definition.bVisible = Override->bVisible;
			if(Override->bOverrideEnabled) Definition.bEnabled = Override->bEnabled;
			if(Override->bOverrideRequiresConfirmation) Definition.bRequiresConfirmation = Override->bRequiresConfirmation;
			if(Override->bOverrideNumberDisplay) Definition.NumberDisplay = Override->NumberDisplay;
			if(Override->bOverrideVisibleConditions) Definition.VisibleConditions = Override->VisibleConditions;
			if(Override->bOverrideEnableConditions) Definition.EnableConditions = Override->EnableConditions;
		}
	}

	TSet<FSettingId> UniqueIds;
	for(const FSettingDefinition& Definition : FinalDefinitions)
	{
		ensureEditorMsgf(Definition.SettingId.IsValid() && !UniqueIds.Contains(Definition.SettingId), FString::Printf(TEXT("Duplicate or invalid setting id: %s"), *Definition.SettingId.Name.ToString()), EDC_Default, EDV_Error);
		UniqueIds.Add(Definition.SettingId);
	}
	FSettingModuleSaveData ValidationData;
	for(const FSettingDefinition& Definition : FinalDefinitions)
	{
		ensureEditorMsgf(!Definition.Page.IsNone(), FString::Printf(TEXT("Setting %s has no page."), *Definition.SettingId.Name.ToString()), EDC_Default, EDV_Error);
		if(!FindSettingProvider(Definition))
		{
			FResolvedSettingProperty Resolved;
			ensureEditorMsgf(ResolvePropertyPath(FSettingModuleSaveData::StaticStruct(), &ValidationData, Definition.SourcePath, Resolved), FString::Printf(TEXT("Setting %s has invalid source path %s."), *Definition.SettingId.Name.ToString(), *Definition.SourcePath), EDC_Default, EDV_Error);
		}

		for(int32 OptionIndex = 0; OptionIndex < Definition.Options.Num(); ++OptionIndex)
		{
			ensureEditorMsgf(!Definition.Options.ContainsByPredicate([&Definition, OptionIndex](const FSettingOption& Option)
			{
				return &Option != &Definition.Options[OptionIndex] && Option.Value == Definition.Options[OptionIndex].Value;
			}), FString::Printf(TEXT("Setting %s has duplicate option values."), *Definition.SettingId.Name.ToString()), EDC_Default, EDV_Error);
		}
		for(const FSettingCondition& Condition : Definition.VisibleConditions)
		{
			ensureEditorMsgf(UniqueIds.Contains(Condition.OtherSetting), FString::Printf(TEXT("Setting %s references missing visible condition %s."), *Definition.SettingId.Name.ToString(), *Condition.OtherSetting.Name.ToString()), EDC_Default, EDV_Error);
		}
		for(const FSettingCondition& Condition : Definition.EnableConditions)
		{
			ensureEditorMsgf(UniqueIds.Contains(Condition.OtherSetting), FString::Printf(TEXT("Setting %s references missing enable condition %s."), *Definition.SettingId.Name.ToString(), *Condition.OtherSetting.Name.ToString()), EDC_Default, EDV_Error);
		}
	}

	FinalDefinitions.Sort([](const FSettingDefinition& A, const FSettingDefinition& B)
	{
		if(A.Page != B.Page) return A.Page.LexicalLess(B.Page);
		if(A.Category != B.Category) return A.Category.LexicalLess(B.Category);
		return A.Order < B.Order;
	});

#if WITH_EDITOR
	if(bUpdateSnapshot && Registry)
	{
		Registry->Modify();
		Registry->FinalDefinitions = FinalDefinitions;
		++Registry->GeneratedSnapshotVersion;
		Registry->MarkPackageDirty();
	}
#endif

	if(EditSession.bActive)
	{
		BuildSettingEntries();
	}
}

bool USettingModule::ResolvePropertyPath(UStruct* InRootStruct, void* InRootData, const FString& InPath, FResolvedSettingProperty& OutResolved) const
{
	TArray<FString> Segments;
	InPath.ParseIntoArray(Segments, TEXT("."), true);
	UStruct* CurrentStruct = InRootStruct;
	void* CurrentContainer = InRootData;
	for(int32 Index = 0; Index < Segments.Num(); ++Index)
	{
		FProperty* Property = FindFProperty<FProperty>(CurrentStruct, *Segments[Index]);
		if(!Property)
		{
			return false;
		}
		if(Index == Segments.Num() - 1)
		{
			OutResolved.ContainerPtr = CurrentContainer;
			OutResolved.Property = Property;
			return true;
		}

		FStructProperty* StructProperty = CastField<FStructProperty>(Property);
		if(!StructProperty)
		{
			return false;
		}
		CurrentContainer = StructProperty->ContainerPtrToValuePtr<void>(CurrentContainer);
		CurrentStruct = StructProperty->Struct;
	}
	return false;
}

FParameter USettingModule::ReadPropertyValue(const FResolvedSettingProperty& InResolved) const
{
	FProperty* Property = InResolved.Property;
	void* ValuePtr = Property ? Property->ContainerPtrToValuePtr<void>(InResolved.ContainerPtr) : nullptr;
	if(!ValuePtr) return FParameter();
	if(const FBoolProperty* Typed = CastField<FBoolProperty>(Property)) return FParameter(Typed->GetPropertyValue(ValuePtr));
	if(const FIntProperty* Typed = CastField<FIntProperty>(Property)) return FParameter(Typed->GetPropertyValue(ValuePtr));
	if(const FInt64Property* Typed = CastField<FInt64Property>(Property)) return FParameter(Typed->GetPropertyValue(ValuePtr));
	if(const FFloatProperty* Typed = CastField<FFloatProperty>(Property)) return FParameter(Typed->GetPropertyValue(ValuePtr));
	if(const FDoubleProperty* Typed = CastField<FDoubleProperty>(Property)) return FParameter(Typed->GetPropertyValue(ValuePtr));
	if(const FNameProperty* Typed = CastField<FNameProperty>(Property)) return FParameter(Typed->GetPropertyValue(ValuePtr));
	if(const FStrProperty* Typed = CastField<FStrProperty>(Property)) return FParameter(Typed->GetPropertyValue(ValuePtr));
	if(const FTextProperty* Typed = CastField<FTextProperty>(Property)) return FParameter(Typed->GetPropertyValue(ValuePtr));
	if(const FEnumProperty* Typed = CastField<FEnumProperty>(Property)) return FParameter(Typed->GetUnderlyingProperty()->GetSignedIntPropertyValue(ValuePtr));
	if(const FByteProperty* Typed = CastField<FByteProperty>(Property)) return FParameter(static_cast<int64>(Typed->GetPropertyValue(ValuePtr)));
	if(const FStructProperty* Typed = CastField<FStructProperty>(Property); Typed && Typed->Struct == TBaseStructure<FKey>::Get()) return FParameter(*static_cast<FKey*>(ValuePtr));
	if(const FStructProperty* Typed = CastField<FStructProperty>(Property); Typed && Typed->Struct == TBaseStructure<FIntPoint>::Get()) return FParameter(*static_cast<FIntPoint*>(ValuePtr));
	return FParameter();
}

bool USettingModule::WritePropertyValue(const FResolvedSettingProperty& InResolved, const FParameter& InValue) const
{
	FProperty* Property = InResolved.Property;
	void* ValuePtr = Property ? Property->ContainerPtrToValuePtr<void>(InResolved.ContainerPtr) : nullptr;
	if(!ValuePtr) return false;
	if(FBoolProperty* Typed = CastField<FBoolProperty>(Property); Typed && InValue.Is<bool>()) { Typed->SetPropertyValue(ValuePtr, InValue.Get<bool>()); return true; }
	if(FIntProperty* Typed = CastField<FIntProperty>(Property); Typed && InValue.Is<int32>()) { Typed->SetPropertyValue(ValuePtr, InValue.Get<int32>()); return true; }
	if(FInt64Property* Typed = CastField<FInt64Property>(Property); Typed && InValue.Is<int64>()) { Typed->SetPropertyValue(ValuePtr, InValue.Get<int64>()); return true; }
	if(FFloatProperty* Typed = CastField<FFloatProperty>(Property); Typed && InValue.Is<float>()) { Typed->SetPropertyValue(ValuePtr, InValue.Get<float>()); return true; }
	if(FDoubleProperty* Typed = CastField<FDoubleProperty>(Property); Typed && InValue.Is<double>()) { Typed->SetPropertyValue(ValuePtr, InValue.Get<double>()); return true; }
	if(FNameProperty* Typed = CastField<FNameProperty>(Property); Typed && InValue.Is<FName>()) { Typed->SetPropertyValue(ValuePtr, InValue.Get<FName>()); return true; }
	if(FStrProperty* Typed = CastField<FStrProperty>(Property); Typed && InValue.Is<FString>()) { Typed->SetPropertyValue(ValuePtr, InValue.Get<FString>()); return true; }
	if(FTextProperty* Typed = CastField<FTextProperty>(Property); Typed && InValue.Is<FText>()) { Typed->SetPropertyValue(ValuePtr, InValue.Get<FText>()); return true; }
	if(FEnumProperty* Typed = CastField<FEnumProperty>(Property); Typed && InValue.Is<int64>()) { Typed->GetUnderlyingProperty()->SetIntPropertyValue(ValuePtr, InValue.Get<int64>()); return true; }
	if(FByteProperty* Typed = CastField<FByteProperty>(Property); Typed && InValue.Is<int64>()) { Typed->SetPropertyValue(ValuePtr, static_cast<uint8>(InValue.Get<int64>())); return true; }
	if(const FStructProperty* Typed = CastField<FStructProperty>(Property); Typed && Typed->Struct == TBaseStructure<FKey>::Get() && InValue.Is<FKey>()) { *static_cast<FKey*>(ValuePtr) = InValue.Get<FKey>(); return true; }
	if(const FStructProperty* Typed = CastField<FStructProperty>(Property); Typed && Typed->Struct == TBaseStructure<FIntPoint>::Get() && InValue.Is<FIntPoint>()) { *static_cast<FIntPoint*>(ValuePtr) = InValue.Get<FIntPoint>(); return true; }
	return false;
}

const FSettingDefinition* USettingModule::FindSettingDefinition(FSettingId InSettingId) const
{
	return FinalDefinitions.FindByPredicate([InSettingId](const FSettingDefinition& Definition)
	{
		return Definition.SettingId == InSettingId;
	});
}

USettingProviderBase* USettingModule::FindSettingProvider(const FSettingDefinition& InDefinition) const
{
	for(USettingProviderBase* Provider : Providers)
	{
		if(Provider && Provider->CanHandle(InDefinition))
		{
			return Provider;
		}
	}
	return nullptr;
}

FSettingValidationResult USettingModule::ValidateSettingValue(const FSettingDefinition& InDefinition, const FParameter& InValue) const
{
	if(!InValue.HasValue())
	{
		return FSettingValidationResult::Invalid(NSLOCTEXT("WH.SettingModule", "MissingValue", "设置值不能为空。"));
	}

	const FParameter CurrentValue = GetPendingValue(InDefinition.SettingId);
	if(CurrentValue.HasValue() && CurrentValue.GetValueStruct() != InValue.GetValueStruct())
	{
		return FSettingValidationResult::Invalid(NSLOCTEXT("WH.SettingModule", "InvalidType", "设置值类型不匹配。"));
	}

	if(!InDefinition.Options.IsEmpty() && !InDefinition.Options.ContainsByPredicate([&InValue](const FSettingOption& Option)
	{
		return Option.Value == InValue;
	}))
	{
		return FSettingValidationResult::Invalid(NSLOCTEXT("WH.SettingModule", "InvalidOption", "该值不在可选项中。"));
	}

	double NumericValue = 0.0;
	if(TryGetNumericValue(InValue, NumericValue))
	{
		if(InDefinition.NumberDisplay.bHasMin && NumericValue < InDefinition.NumberDisplay.Min)
		{
			return FSettingValidationResult::Invalid(FText::Format(
				NSLOCTEXT("WH.SettingModule", "BelowMinimum", "设置值不能小于 {0}。"),
				FText::AsNumber(InDefinition.NumberDisplay.Min)));
		}
		if(InDefinition.NumberDisplay.bHasMax && NumericValue > InDefinition.NumberDisplay.Max)
		{
			return FSettingValidationResult::Invalid(FText::Format(
				NSLOCTEXT("WH.SettingModule", "AboveMaximum", "设置值不能大于 {0}。"),
				FText::AsNumber(InDefinition.NumberDisplay.Max)));
		}
	}

	if(const USettingProviderBase* Provider = FindSettingProvider(InDefinition))
	{
		return Provider->Validate(InDefinition, InValue);
	}
	return FSettingValidationResult::Valid();
}

bool USettingModule::EvaluateConditions(const TArray<FSettingCondition>& InConditions) const
{
	for(const FSettingCondition& Condition : InConditions)
	{
		const FParameter OtherValue = GetPendingValue(Condition.OtherSetting);
		bool bPassed = false;
		switch(Condition.Op)
		{
			case ESettingConditionOp::Equals:
				bPassed = OtherValue == Condition.Value;
				break;
			case ESettingConditionOp::NotEquals:
				bPassed = OtherValue != Condition.Value;
				break;
			case ESettingConditionOp::Greater:
			case ESettingConditionOp::Less:
			{
				double Left = 0.0;
				double Right = 0.0;
				if(TryGetNumericValue(OtherValue, Left) && TryGetNumericValue(Condition.Value, Right))
				{
					bPassed = Condition.Op == ESettingConditionOp::Greater ? Left > Right : Left < Right;
				}
				break;
			}
		}
		if(!bPassed)
		{
			return false;
		}
	}
	return true;
}

FParameter USettingModule::ReadSessionValue(FSettingModuleSaveData& InData, FSettingId InSettingId) const
{
	const FSettingDefinition* Definition = FindSettingDefinition(InSettingId);
	FResolvedSettingProperty Resolved;
	return Definition && ResolvePropertyPath(FSettingModuleSaveData::StaticStruct(), &InData, Definition->SourcePath, Resolved) ? ReadPropertyValue(Resolved) : FParameter();
}

bool USettingModule::WriteSessionValue(FSettingModuleSaveData& InData, FSettingId InSettingId, const FParameter& InValue) const
{
	const FSettingDefinition* Definition = FindSettingDefinition(InSettingId);
	FResolvedSettingProperty Resolved;
	return Definition && ResolvePropertyPath(FSettingModuleSaveData::StaticStruct(), &InData, Definition->SourcePath, Resolved) && WritePropertyValue(Resolved, InValue);
}

FSettingModuleSaveData USettingModule::GetCurrentCombinedSettings() const
{
	FSettingModuleSaveData Data;
	Data.WidgetData = UWidgetModule::Get().GetSaveDataRef<FWidgetModuleSaveData>(true);
	Data.AudioData = UAudioModule::Get().GetSaveDataRef<FAudioModuleSaveData>(true);
	Data.VideoData = UVideoModule::Get().GetSaveDataRef<FVideoModuleSaveData>(true);
	Data.CameraData = UCameraModule::Get().GetSaveDataRef<FCameraModuleSaveData>(true);
	Data.InputData = UInputModule::Get().GetSaveDataRef<FInputModuleSaveData>(false);
	Data.ParameterData = UParameterModule::Get().GetSaveDataRef<FParameterModuleSaveData>(true);
	return Data;
}

FSettingModuleSaveData USettingModule::GetDefaultCombinedSettings() const
{
	if(const USettingSaveGame* SaveGame = Cast<USettingSaveGame>(GetModuleSaveGame()))
	{
		return const_cast<USettingSaveGame*>(SaveGame)->GetDefaultDataRef<FSettingModuleSaveData>();
	}
	return FSettingModuleSaveData();
}

void USettingModule::ApplyCombinedSettings(FSettingModuleSaveData& InData)
{
	UWidgetModule::Get().LoadSaveData(&InData.WidgetData, EPhase::All);
	UAudioModule::Get().LoadSaveData(&InData.AudioData, EPhase::All);
	UVideoModule::Get().LoadSaveData(&InData.VideoData, EPhase::All);
	UCameraModule::Get().LoadSaveData(&InData.CameraData, EPhase::All);
	UInputModule::Get().LoadSaveData(&InData.InputData, EPhase::All);
	UParameterModule::Get().LoadSaveData(&InData.ParameterData, EPhase::All);
}

void USettingModule::SaveSettings()
{
	if(USettingSaveGame* SaveGame = Cast<USettingSaveGame>(GetModuleSaveGame()))
	{
		SaveGame->Save(true);
	}
}

void USettingModule::BuildSettingEntries()
{
	SettingEntries.Reset();
	SettingEntryMap.Reset();
	for(const FSettingDefinition& Definition : FinalDefinitions)
	{
		USettingEntry* Entry = NewObject<USettingEntry>(this);
		Entry->Initialize(this, Definition);
		SettingEntries.Add(Entry);
		SettingEntryMap.Add(Definition.SettingId, Entry);
	}
}

void USettingModule::RefreshSettingDefinitions()
{
	BuildSettingDefinitions(true);
}

#if WITH_EDITOR
void USettingModule::GenerateRegistrySnapshot(USettingRegistry* InRegistry)
{
	if(!InRegistry)
	{
		return;
	}

	USettingRegistry* PreviousRegistry = Registry;
	Registry = InRegistry;
	BuildSettingDefinitions(true);
	Registry = PreviousRegistry;
	if(PreviousRegistry != InRegistry)
	{
		BuildSettingDefinitions();
	}
}
#endif

void USettingModule::BeginEdit()
{
	if(EditSession.bActive)
	{
		return;
	}

	EditSession.AppliedData = GetCurrentCombinedSettings();
	EditSession.PendingData = EditSession.AppliedData;
	EditSession.DefaultData = GetDefaultCombinedSettings();
	EditSession.bActive = true;
	ConfirmationTransaction = FSettingConfirmationTransaction();
	ValidationResults.Reset();
	for(USettingProviderBase* Provider : Providers)
	{
		if(Provider)
		{
			Provider->BeginEdit(FinalDefinitions);
		}
	}
	BuildSettingEntries();
}

void USettingModule::EndEdit()
{
	if(!EditSession.bActive)
	{
		return;
	}
	if(ConfirmationTransaction.bActive)
	{
		RejectPendingSettings();
	}
	for(USettingProviderBase* Provider : Providers)
	{
		if(Provider)
		{
			Provider->EndEdit();
		}
	}
	EditSession = FSettingEditSession();
	SettingEntries.Reset();
	SettingEntryMap.Reset();
	ValidationResults.Reset();
}

bool USettingModule::ApplyEditSession()
{
	if(!EditSession.bActive || ConfirmationTransaction.bActive)
	{
		return false;
	}

	TArray<const FSettingDefinition*> DirtyDefinitions;
	bool bRequiresConfirmation = false;
	for(const FSettingDefinition& Definition : FinalDefinitions)
	{
		if(!IsSettingDirty(Definition.SettingId))
		{
			continue;
		}
		const FSettingValidationResult Validation = ValidateSettingValue(Definition, GetPendingValue(Definition.SettingId));
		ValidationResults.Add(Definition.SettingId, Validation);
		if(!Validation.bValid)
		{
			return false;
		}
		DirtyDefinitions.Add(&Definition);
		bRequiresConfirmation |= Definition.bRequiresConfirmation;
	}
	if(DirtyDefinitions.IsEmpty())
	{
		return false;
	}

	ApplyCombinedSettings(EditSession.PendingData);
	for(const FSettingDefinition* Definition : DirtyDefinitions)
	{
		if(USettingProviderBase* Provider = FindSettingProvider(*Definition))
		{
			if(!Provider->Apply(*Definition, GetPendingValue(Definition->SettingId)))
			{
				ApplyCombinedSettings(EditSession.AppliedData);
				for(const FSettingDefinition* AppliedDefinition : DirtyDefinitions)
				{
					if(USettingProviderBase* AppliedProvider = FindSettingProvider(*AppliedDefinition))
					{
						AppliedProvider->Rollback(*AppliedDefinition);
					}
				}
				return false;
			}
		}
	}

	if(bRequiresConfirmation)
	{
		ConfirmationTransaction.PreviousData = EditSession.AppliedData;
		ConfirmationTransaction.SettingIds.Reset();
		for(const FSettingDefinition* Definition : DirtyDefinitions)
		{
			if(Definition->bRequiresConfirmation)
			{
				ConfirmationTransaction.SettingIds.Add(Definition->SettingId);
			}
		}
		ConfirmationTransaction.ExpiresAt = FPlatformTime::Seconds() + ConfirmationTimeout;
		ConfirmationTransaction.bActive = true;
		return true;
	}

	EditSession.AppliedData = EditSession.PendingData;
	for(USettingProviderBase* Provider : Providers)
	{
		if(Provider)
		{
			Provider->Commit();
		}
	}
	SaveSettings();
	ValidationResults.Reset();
	return true;
}

void USettingModule::CancelEditSession()
{
	if(!EditSession.bActive)
	{
		return;
	}
	if(ConfirmationTransaction.bActive)
	{
		RejectPendingSettings();
		return;
	}

	ApplyCombinedSettings(EditSession.AppliedData);
	for(USettingProviderBase* Provider : Providers)
	{
		if(!Provider)
		{
			continue;
		}
		for(const FSettingDefinition& Definition : FinalDefinitions)
		{
			if(Provider->CanHandle(Definition))
			{
				Provider->Rollback(Definition);
			}
		}
	}
	EditSession.PendingData = EditSession.AppliedData;
	ValidationResults.Reset();
	for(const FSettingDefinition& Definition : FinalDefinitions)
	{
		OnSettingValueChanged.Broadcast(Definition.SettingId);
	}
}

bool USettingModule::ConfirmPendingSettings()
{
	if(!EditSession.bActive || !ConfirmationTransaction.bActive)
	{
		return false;
	}

	EditSession.AppliedData = EditSession.PendingData;
	for(USettingProviderBase* Provider : Providers)
	{
		if(Provider)
		{
			Provider->Commit();
		}
	}
	ConfirmationTransaction = FSettingConfirmationTransaction();
	ValidationResults.Reset();
	SaveSettings();
	return true;
}

void USettingModule::RejectPendingSettings()
{
	if(!EditSession.bActive || !ConfirmationTransaction.bActive)
	{
		return;
	}

	ApplyCombinedSettings(ConfirmationTransaction.PreviousData);
	for(USettingProviderBase* Provider : Providers)
	{
		if(!Provider)
		{
			continue;
		}
		for(const FSettingDefinition& Definition : FinalDefinitions)
		{
			if(Provider->CanHandle(Definition))
			{
				Provider->Rollback(Definition);
			}
		}
	}
	EditSession.PendingData = EditSession.AppliedData;
	ConfirmationTransaction = FSettingConfirmationTransaction();
	ValidationResults.Reset();
	for(const FSettingDefinition& Definition : FinalDefinitions)
	{
		OnSettingValueChanged.Broadcast(Definition.SettingId);
	}
}

void USettingModule::ResetAllToDefault()
{
	if(!EditSession.bActive) return;
	for(const FSettingDefinition& Definition : FinalDefinitions)
	{
		SetPendingValue(Definition.SettingId, GetDefaultValue(Definition.SettingId));
	}
}

bool USettingModule::CanApply() const
{
	if(!EditSession.bActive || ConfirmationTransaction.bActive) return false;
	for(const FSettingDefinition& Definition : FinalDefinitions)
	{
		if(IsSettingDirty(Definition.SettingId)) return true;
	}
	return false;
}

bool USettingModule::CanReset() const
{
	if(!EditSession.bActive) return false;
	for(const FSettingDefinition& Definition : FinalDefinitions)
	{
		if(CanResetSetting(Definition.SettingId)) return true;
	}
	return false;
}

bool USettingModule::IsSettingDirty(FSettingId InSettingId) const
{
	if(!EditSession.bActive) return false;
	return GetAppliedValue(InSettingId) != GetPendingValue(InSettingId);
}

bool USettingModule::CanResetSetting(FSettingId InSettingId) const
{
	if(!EditSession.bActive) return false;
	return GetPendingValue(InSettingId) != GetDefaultValue(InSettingId);
}

bool USettingModule::IsSettingEnabled(FSettingId InSettingId) const
{
	if(const FSettingDefinition* Definition = FindSettingDefinition(InSettingId))
	{
		return Definition->bEnabled && EvaluateConditions(Definition->EnableConditions);
	}
	return false;
}

bool USettingModule::IsSettingVisible(FSettingId InSettingId) const
{
	if(const FSettingDefinition* Definition = FindSettingDefinition(InSettingId))
	{
		return Definition->bVisible && EvaluateConditions(Definition->VisibleConditions);
	}
	return false;
}

FSettingValidationResult USettingModule::GetValidationResult(FSettingId InSettingId) const
{
	if(const FSettingValidationResult* Result = ValidationResults.Find(InSettingId))
	{
		return *Result;
	}
	return FSettingValidationResult::Valid();
}

FParameter USettingModule::GetAppliedValue(FSettingId InSettingId) const
{
	if(!EditSession.bActive) return FParameter();
	if(const FSettingDefinition* Definition = FindSettingDefinition(InSettingId))
	{
		if(const USettingProviderBase* Provider = FindSettingProvider(*Definition))
		{
			return Provider->GetAppliedValue(*Definition);
		}
	}
	return const_cast<USettingModule*>(this)->ReadSessionValue(const_cast<FSettingModuleSaveData&>(EditSession.AppliedData), InSettingId);
}

FParameter USettingModule::GetPendingValue(FSettingId InSettingId) const
{
	if(!EditSession.bActive) return FParameter();
	if(const FSettingDefinition* Definition = FindSettingDefinition(InSettingId))
	{
		if(const USettingProviderBase* Provider = FindSettingProvider(*Definition))
		{
			return Provider->GetPendingValue(*Definition);
		}
	}
	return const_cast<USettingModule*>(this)->ReadSessionValue(const_cast<FSettingModuleSaveData&>(EditSession.PendingData), InSettingId);
}

FParameter USettingModule::GetDefaultValue(FSettingId InSettingId) const
{
	if(!EditSession.bActive)
	{
		return FParameter();
	}
	if(const FSettingDefinition* Definition = FindSettingDefinition(InSettingId))
	{
		if(const USettingProviderBase* Provider = FindSettingProvider(*Definition))
		{
			return Provider->GetDefaultValue(*Definition);
		}
	}
	return const_cast<USettingModule*>(this)->ReadSessionValue(const_cast<FSettingModuleSaveData&>(EditSession.DefaultData), InSettingId);
}

bool USettingModule::SetPendingValue(FSettingId InSettingId, const FParameter& InValue)
{
	if(!EditSession.bActive || ConfirmationTransaction.bActive) return false;
	const FSettingDefinition* Definition = FindSettingDefinition(InSettingId);
	if(!Definition || !IsSettingEnabled(InSettingId)) return false;

	const FSettingValidationResult Validation = ValidateSettingValue(*Definition, InValue);
	ValidationResults.Add(InSettingId, Validation);
	if(!Validation.bValid)
	{
		return false;
	}

	if(USettingProviderBase* Provider = FindSettingProvider(*Definition))
	{
		const FParameter PreviousValue = Provider->GetPendingValue(*Definition);
		if(!Provider->SetPendingValue(*Definition, InValue))
		{
			return false;
		}
		if(Definition->ApplyPolicy == ESettingApplyPolicy::Preview && !Provider->Preview(*Definition, InValue))
		{
			Provider->SetPendingValue(*Definition, PreviousValue);
			return false;
		}
		if(Definition->ApplyPolicy == ESettingApplyPolicy::Immediate)
		{
			if(!Provider->Apply(*Definition, InValue))
			{
				Provider->SetPendingValue(*Definition, PreviousValue);
				return false;
			}
			Provider->Commit();
			SaveSettings();
		}
		OnSettingValueChanged.Broadcast(InSettingId);
		return true;
	}

	if(!WriteSessionValue(EditSession.PendingData, InSettingId, InValue)) return false;
	if(Definition->ApplyPolicy != ESettingApplyPolicy::Deferred)
	{
		FSettingModuleSaveData RuntimeData = GetCurrentCombinedSettings();
		WriteSessionValue(RuntimeData, InSettingId, InValue);
		ApplyCombinedSettings(RuntimeData);
		if(Definition->ApplyPolicy == ESettingApplyPolicy::Immediate)
		{
			WriteSessionValue(EditSession.AppliedData, InSettingId, InValue);
			SaveSettings();
		}
	}
	OnSettingValueChanged.Broadcast(InSettingId);
	return true;
}

TArray<USettingEntry*> USettingModule::GetSettingEntries() const
{
	TArray<USettingEntry*> Result;
	Result.Reserve(SettingEntries.Num());
	for(USettingEntry* Entry : SettingEntries)
	{
		Result.Add(Entry);
	}
	return Result;
}

TArray<USettingEntry*> USettingModule::GetSettingEntriesByPage(FName InPage) const
{
	TArray<USettingEntry*> Result;
	for(USettingEntry* Entry : SettingEntries)
	{
		if(Entry && Entry->GetDefinition().Page == InPage)
		{
			Result.Add(Entry);
		}
	}
	return Result;
}

TArray<FSettingPageDefinition> USettingModule::GetSettingPages() const
{
	TArray<FSettingPageDefinition> Result = Registry ? Registry->Pages : TArray<FSettingPageDefinition>();
	for(const FSettingDefinition& Definition : FinalDefinitions)
	{
		if(Definition.Page.IsNone() || Result.ContainsByPredicate([&Definition](const FSettingPageDefinition& Page)
		{
			return Page.Page == Definition.Page;
		}))
		{
			continue;
		}

		FSettingPageDefinition& Page = Result.AddDefaulted_GetRef();
		Page.Page = Definition.Page;
		Page.DisplayName = FText::FromName(Definition.Page);
		Page.Order = Result.Num() - 1;
	}
	Result.Sort([](const FSettingPageDefinition& A, const FSettingPageDefinition& B)
	{
		return A.Order == B.Order ? A.Page.LexicalLess(B.Page) : A.Order < B.Order;
	});
	return Result;
}

void USettingModule::LoadData(FSaveData* InSaveData, EPhase InPhase)
{
	auto& SaveData = InSaveData->CastRef<FSettingModuleSaveData>();

	UWidgetModule::Get().LoadSaveData(&SaveData.WidgetData, InPhase);
	UAudioModule::Get().LoadSaveData(&SaveData.AudioData, InPhase);
	UVideoModule::Get().LoadSaveData(&SaveData.VideoData, InPhase);
	UCameraModule::Get().LoadSaveData(&SaveData.CameraData, InPhase);
	UInputModule::Get().LoadSaveData(&SaveData.InputData, InPhase);
	UParameterModule::Get().LoadSaveData(&SaveData.ParameterData, InPhase);
}

void USettingModule::UnloadData(EPhase InPhase)
{
}

FSaveData* USettingModule::ToData()
{
	FSettingModuleSaveData& SaveData = GetMutableSaveData<FSettingModuleSaveData>();
	SaveData = FSettingModuleSaveData();

	SaveData.WidgetData = UWidgetModule::Get().GetSaveDataRef<FWidgetModuleSaveData>(true);
	SaveData.AudioData = UAudioModule::Get().GetSaveDataRef<FAudioModuleSaveData>(true);
	SaveData.VideoData = UVideoModule::Get().GetSaveDataRef<FVideoModuleSaveData>(true);
	SaveData.CameraData = UCameraModule::Get().GetSaveDataRef<FCameraModuleSaveData>(true);
	SaveData.InputData = UInputModule::Get().GetSaveDataRef<FInputModuleSaveData>(false);
	SaveData.ParameterData = UParameterModule::Get().GetSaveDataRef<FParameterModuleSaveData>(true);
	
	return &SaveData;
}

void USettingModule::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
}
