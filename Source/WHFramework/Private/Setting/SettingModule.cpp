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
#include "Setting/InputSettingProvider.h"
#include "Setting/SettingProviderBase.h"
#include "Setting/SettingRegistry.h"
#include "Video/VideoModule.h"
#include "Widget/WidgetModule.h"
#include "UObject/UnrealType.h"

IMPLEMENTATION_MODULE(USettingModule)

// Sets default values
USettingModule::USettingModule()
{
	ModuleName = FName("SettingModule");
	ModuleDisplayName = FText::FromString(TEXT("Setting Module"));

	ModuleSaveGame = USettingSaveGame::StaticClass();
	
	ModuleNetworkComponent = USettingModuleNetworkComponent::StaticClass();

	Providers.Add(CreateDefaultSubobject<UInputSettingProvider>(TEXT("InputSettingProvider")));
}

USettingModule::~USettingModule()
{
	TERMINATION_MODULE(USettingModule)
}

#if WITH_EDITOR
void USettingModule::OnGenerate()
{
	Super::OnGenerate();
	BuildSettingDefinitions();
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
		SettingEntries.Reset();
		SettingEntryMap.Reset();
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
		Definition.Page = FName(*Root);
		Definition.Category = Property->HasMetaData(TEXT("SettingCategory")) ? FName(*Property->GetMetaData(TEXT("SettingCategory"))) : FName(TEXT("General"));
		Definition.Order = Property->HasMetaData(TEXT("SettingOrder")) ? FCString::Atoi(*Property->GetMetaData(TEXT("SettingOrder"))) : OutDefinitions.Num() - 1;
		Definition.DisplayName = Property->GetDisplayNameText();
		Definition.Description = Property->GetToolTipText();
		Definition.Renderer = InferRenderer(Property);

		const FString ApplyPolicy = Property->GetMetaData(TEXT("SettingApply"));
		Definition.ApplyPolicy = ApplyPolicy.Equals(TEXT("Preview"), ESearchCase::IgnoreCase)
			? ESettingApplyPolicy::Preview
			: ApplyPolicy.Equals(TEXT("Immediate"), ESearchCase::IgnoreCase)
				? ESettingApplyPolicy::Immediate
				: ESettingApplyPolicy::Deferred;

		if(Property->HasMetaData(TEXT("ClampMin")))
		{
			Definition.NumberDisplay.Min = FCString::Atod(*Property->GetMetaData(TEXT("ClampMin")));
		}
		if(Property->HasMetaData(TEXT("ClampMax")))
		{
			Definition.NumberDisplay.Max = FCString::Atod(*Property->GetMetaData(TEXT("ClampMax")));
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
		return StructProperty->Struct == TBaseStructure<FKey>::Get();
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

void USettingModule::BuildSettingDefinitions()
{
	FinalDefinitions.Reset();
	CollectPropertyDefinitions(FSettingModuleSaveData::StaticStruct(), FString(), FinalDefinitions);
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
			if(Override->bOverridePage) Definition.Page = Override->Page;
			if(Override->bOverrideCategory) Definition.Category = Override->Category;
			if(Override->bOverrideOrder) Definition.Order = Override->Order;
			if(Override->bOverrideRenderer) Definition.Renderer = Override->Renderer;
			if(Override->bOverrideApplyPolicy) Definition.ApplyPolicy = Override->ApplyPolicy;
			if(Override->bOverrideVisible) Definition.bVisible = Override->bVisible;
		}
	}

	TSet<FSettingId> UniqueIds;
	for(const FSettingDefinition& Definition : FinalDefinitions)
	{
		ensureEditorMsgf(Definition.SettingId.IsValid() && !UniqueIds.Contains(Definition.SettingId), FString::Printf(TEXT("Duplicate or invalid setting id: %s"), *Definition.SettingId.Name.ToString()), EDC_Default, EDV_Error);
		UniqueIds.Add(Definition.SettingId);
	}

	FinalDefinitions.Sort([](const FSettingDefinition& A, const FSettingDefinition& B)
	{
		if(A.Page != B.Page) return A.Page.LexicalLess(B.Page);
		if(A.Category != B.Category) return A.Category.LexicalLess(B.Category);
		return A.Order < B.Order;
	});
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
	return false;
}

const FSettingDefinition* USettingModule::FindSettingDefinition(FSettingId InSettingId) const
{
	return FinalDefinitions.FindByPredicate([InSettingId](const FSettingDefinition& Definition)
	{
		return Definition.SettingId == InSettingId;
	});
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

void USettingModule::ApplyCombinedSettings(FSettingModuleSaveData& InData)
{
	UWidgetModule::Get().LoadSaveData(&InData.WidgetData, EPhase::All);
	UAudioModule::Get().LoadSaveData(&InData.AudioData, EPhase::All);
	UVideoModule::Get().LoadSaveData(&InData.VideoData, EPhase::All);
	UCameraModule::Get().LoadSaveData(&InData.CameraData, EPhase::All);
	UInputModule::Get().LoadSaveData(&InData.InputData, EPhase::All);
	UParameterModule::Get().LoadSaveData(&InData.ParameterData, EPhase::All);
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
	BuildSettingDefinitions();
}

void USettingModule::BeginEdit()
{
	EditSession.AppliedData = GetCurrentCombinedSettings();
	EditSession.PendingData = EditSession.AppliedData;
	EditSession.DefaultData = FSettingModuleSaveData();
	EditSession.bActive = true;
	BuildSettingEntries();
}

bool USettingModule::ApplyEditSession()
{
	if(!EditSession.bActive) return false;
	ApplyCombinedSettings(EditSession.PendingData);
	for(USettingProviderBase* Provider : Providers)
	{
		if(!Provider) continue;
		for(const FSettingDefinition& Definition : FinalDefinitions)
		{
			if(Provider->CanHandle(Definition)) Provider->Apply(Definition, GetPendingValue(Definition.SettingId));
		}
	}
	EditSession.AppliedData = EditSession.PendingData;
	return true;
}

void USettingModule::CancelEditSession()
{
	if(!EditSession.bActive) return;
	ApplyCombinedSettings(EditSession.AppliedData);
	for(USettingProviderBase* Provider : Providers)
	{
		if(!Provider) continue;
		for(const FSettingDefinition& Definition : FinalDefinitions)
		{
			if(Provider->CanHandle(Definition)) Provider->Rollback(Definition);
		}
	}
	EditSession.PendingData = EditSession.AppliedData;
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
	if(!EditSession.bActive) return false;
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
	return const_cast<USettingModule*>(this)->ReadSessionValue(const_cast<FSettingModuleSaveData&>(EditSession.PendingData), InSettingId) != const_cast<USettingModule*>(this)->ReadSessionValue(const_cast<FSettingModuleSaveData&>(EditSession.DefaultData), InSettingId);
}

FParameter USettingModule::GetAppliedValue(FSettingId InSettingId) const
{
	if(!EditSession.bActive) return FParameter();
	if(const FSettingDefinition* Definition = FindSettingDefinition(InSettingId))
	{
		for(const USettingProviderBase* Provider : Providers)
		{
			if(Provider && Provider->CanHandle(*Definition)) return Provider->GetAppliedValue(*Definition);
		}
	}
	return const_cast<USettingModule*>(this)->ReadSessionValue(const_cast<FSettingModuleSaveData&>(EditSession.AppliedData), InSettingId);
}

FParameter USettingModule::GetPendingValue(FSettingId InSettingId) const
{
	if(!EditSession.bActive) return FParameter();
	if(const FSettingDefinition* Definition = FindSettingDefinition(InSettingId))
	{
		for(const USettingProviderBase* Provider : Providers)
		{
			if(Provider && Provider->CanHandle(*Definition)) return Provider->GetPendingValue(*Definition);
		}
	}
	return const_cast<USettingModule*>(this)->ReadSessionValue(const_cast<FSettingModuleSaveData&>(EditSession.PendingData), InSettingId);
}

FParameter USettingModule::GetDefaultValue(FSettingId InSettingId) const
{
	return EditSession.bActive ? const_cast<USettingModule*>(this)->ReadSessionValue(const_cast<FSettingModuleSaveData&>(EditSession.DefaultData), InSettingId) : FParameter();
}

bool USettingModule::SetPendingValue(FSettingId InSettingId, const FParameter& InValue)
{
	if(!EditSession.bActive) return false;
	const FSettingDefinition* Definition = FindSettingDefinition(InSettingId);
	if(!Definition) return false;

	for(USettingProviderBase* Provider : Providers)
	{
		if(Provider && Provider->CanHandle(*Definition))
		{
			if(!Provider->SetPendingValue(*Definition, InValue)) return false;
			if(Definition->ApplyPolicy == ESettingApplyPolicy::Preview) Provider->Preview(*Definition, InValue);
			if(Definition->ApplyPolicy == ESettingApplyPolicy::Immediate) Provider->Apply(*Definition, InValue);
			return true;
		}
	}

	if(!WriteSessionValue(EditSession.PendingData, InSettingId, InValue)) return false;
	if(Definition->ApplyPolicy != ESettingApplyPolicy::Deferred)
	{
		ApplyCombinedSettings(EditSession.PendingData);
		if(Definition->ApplyPolicy == ESettingApplyPolicy::Immediate)
		{
			WriteSessionValue(EditSession.AppliedData, InSettingId, InValue);
		}
	}
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
