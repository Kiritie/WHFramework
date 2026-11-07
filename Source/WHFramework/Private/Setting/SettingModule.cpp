// Fill out your copyright notice in the Description page of Project Settings.

#include "Setting/SettingModule.h"
#include "WHFrameworkCoreMacros.h"
#include "Common/LifecycleTypes.h"
#include "Setting/SettingPresentationTypes.h"
#include "Setting/SettingDefinitionTypes.h"
#include "Setting/SettingEditTypes.h"
#include "Setting/SettingPropertyAccess.h"
#include "SaveGame/SaveGameModuleStatics.h"
#include "SaveGame/SaveGameModule.h"

#include "Audio/AudioModule.h"
#include "Camera/CameraModule.h"
#include "Input/InputModule.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Parameter/ParameterModule.h"
#include "Setting/SettingModuleNetworkComponent.h"
#include "Setting/SettingModuleTypes.h"
#include "Setting/SettingEntry.h"
#include "Setting/Provider/InputSettingProvider.h"
#include "Setting/Provider/LanguageSettingProvider.h"
#include "Setting/Provider/ResolutionSettingProvider.h"
#include "Setting/Provider/AudioSettingProvider.h"
#include "Setting/Provider/CameraSettingProvider.h"
#include "Setting/Provider/ParameterSettingProvider.h"
#include "Setting/Provider/SettingProviderBase.h"
#include "Video/VideoModule.h"
#include "Widget/WidgetModule.h"
#include "UObject/UnrealType.h"
#include "Gameplay/WHLocalPlayer.h"
#include "Misc/ScopeExit.h"

IMPLEMENTATION_MODULE(USettingModule)

namespace
{
bool HasMetadata(const FField& Field, const FName Key)
{
#if WITH_METADATA
	return Field.HasMetaData(Key);
#else
	return false;
#endif
}

FString GetMetadata(const FField& Field, const FName Key)
{
#if WITH_METADATA
	return Field.GetMetaData(Key);
#else
	return FString();
#endif
}

FText GetPropertyDisplayName(const FProperty& Property)
{
#if WITH_EDITORONLY_DATA
	return Property.GetDisplayNameText();
#else
	return FText::FromString(FName::NameToDisplayString(Property.GetName(), false));
#endif
}

FText GetPropertyToolTip(const FProperty& Property)
{
#if WITH_EDITORONLY_DATA
	return Property.GetToolTipText();
#else
	return FText::GetEmpty();
#endif
}

bool IsEnumValueHidden(const UEnum& Enum, const int32 Index)
{
#if WITH_METADATA
	return Enum.HasMetaData(TEXT("Hidden"), Index);
#else
	return false;
#endif
}

FText GetEnumValueDisplayName(const UEnum& Enum, const int32 Index)
{
#if WITH_EDITORONLY_DATA
	return Enum.GetDisplayNameTextByIndex(Index);
#else
	return FText::FromString(FName::NameToDisplayString(Enum.GetNameStringByIndex(Index), false));
#endif
}

template <typename TProvider> void EnsureSettingProvider(UObject* InOuter, TArray<TObjectPtr<USettingProviderBase>>& InProviders)
{
	if (InProviders.ContainsByPredicate(
	        [](const TObjectPtr<USettingProviderBase>& Provider)
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
	if (const int32* Value = InValue.GetPtr<int32>())
	{
		OutValue = *Value;
		return true;
	}
	if (const int64* Value = InValue.GetPtr<int64>())
	{
		OutValue = static_cast<double>(*Value);
		return true;
	}
	if (const float* Value = InValue.GetPtr<float>())
	{
		OutValue = *Value;
		return true;
	}
	if (const double* Value = InValue.GetPtr<double>())
	{
		OutValue = *Value;
		return true;
	}
	return false;
}

int32 GetDefaultSettingPageOrder(FName InPage)
{
	if (InPage == FName(TEXT("Audio")))
	{
		return 0;
	}
	if (InPage == FName(TEXT("Video")))
	{
		return 10;
	}
	if (InPage == FName(TEXT("Camera")))
	{
		return 20;
	}
	if (InPage == FName(TEXT("Input")))
	{
		return 30;
	}
	if (InPage == FName(TEXT("Parameter")))
	{
		return 40;
	}
	if (InPage == FName(TEXT("Game")))
	{
		return 50;
	}
	return 100;
}
}

// Sets default values
USettingModule::USettingModule()
{
	ModuleName = FName("SettingModule");
	ModuleDisplayName = FText::FromString(TEXT("Setting Module"));
	SaveScope = ESaveScope::Profile;
	SaveDataVersion = 2;

	ModuleNetworkComponent = USettingModuleNetworkComponent::StaticClass();

	Providers.Add(CreateDefaultSubobject<UInputSettingProvider>(TEXT("InputSettingProvider")));
	Providers.Add(CreateDefaultSubobject<UResolutionSettingProvider>(TEXT("ResolutionSettingProvider")));
	Providers.Add(CreateDefaultSubobject<ULanguageSettingProvider>(TEXT("LanguageSettingProvider")));
	Providers.Add(CreateDefaultSubobject<UAudioSettingProvider>(TEXT("AudioSettingProvider")));
	Providers.Add(CreateDefaultSubobject<UCameraSettingProvider>(TEXT("CameraSettingProvider")));
}

USettingModule::~USettingModule()
{
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
	SaveDataVersion = 2;
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

void USettingModule::OnControlRefresh(float RealDeltaSeconds)
{
	Super::OnControlRefresh(RealDeltaSeconds);
	if (EditSession.bActive && !IsEditSessionOwner(EditSessionPlayer.Get()))
	{
		bHostTerminationPending = true;
		if (!FinalizeHostTermination()) UE_LOG(LogTemp, Error, TEXT("已失效的设置会话无法关闭。"));
		return;
	}
	if (bEndEditRequested && EditSession.bActive)
	{
		EndEdit();
		return;
	}
	if (EditSession.bActive && !EditSessionPlayer.IsValid())
	{
		EndEdit();
		return;
	}
	if (ConfirmationTransaction.bActive && FPlatformTime::Seconds() >= ConfirmationTransaction.ExpiresAt)
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

void USettingModule::PrepareForHostTermination()
{
	Super::PrepareForHostTermination();
	if (EditSession.bActive)
	{
		bHostTerminationPending = true;
		bEndEditRequested = true;
		if (!bStartingEdit && !bEndingEdit && !bApplyingEdit && !bCancellingEdit && !FinalizeHostTermination())
			UE_LOG(LogTemp, Error, TEXT("世界关闭前未能恢复本地玩家未提交的设置。"));
	}
}

bool USettingModule::FinalizeHostTermination()
{
	if (!bHostTerminationPending || bFinalizingHostTermination) return !bHostTerminationPending;
	if (bStartingEdit || bEndingEdit || bApplyingEdit || bCancellingEdit) return false;
	TGuardValue<bool> Finalizing(bFinalizingHostTermination, true);
	UWHLocalPlayer* Player = EditSessionLocalPlayer.Get();
	if (::IsValid(Player) && Player->OwnsSettingEdit(this, EditSessionGeneration))
	{
		auto RetainFailure = [&]() { if (::IsValid(Player)) Player->RetainSettingRecovery(this, EditSessionGeneration); return false; };
		const TArray<FSettingDefinition> Definitions = FinalDefinitions;
		for (const FSettingDefinition& Definition : Definitions)
		{
			if (!AppliedUncommittedSettings.Contains(Definition.SettingId)) continue;
			USettingProviderBase* Provider = FindSettingProvider(Definition);
			if (!Provider) return RetainFailure();
			if (!Provider->CanRollbackForHostTermination(Definition, Player))
			{
				AppliedUncommittedSettings.Remove(Definition.SettingId);
				continue;
			}
			if (!Provider->RollbackForHostTermination(Definition, Player)) return RetainFailure();
			if (!::IsValid(Player) || !Player->OwnsSettingEdit(this, EditSessionGeneration)) return RetainFailure();
			AppliedUncommittedSettings.Remove(Definition.SettingId);
		}
		if (!AppliedUncommittedSettings.IsEmpty()) return RetainFailure();
		Player->ReleaseSettingEdit(this, EditSessionGeneration);
		Player->ClearSettingRecovery(this);
	}
	// 玩家已销毁或租约已被较新的档案操作替换时，不覆盖其后续状态。
	for (USettingProviderBase* Provider : Providers) if (Provider) Provider->EndEdit();
	AppliedUncommittedSettings.Reset();
	EditSession = FSettingEditSession();
	EditSessionPlayer.Reset();
	EditSessionLocalPlayer.Reset();
	EditSessionGeneration = 0;
	ConfirmationTransaction = FSettingConfirmationTransaction();
	SettingEntries.Reset();
	SettingEntryMap.Reset();
	ValidationResults.Reset();
	bEndEditRequested = false;
	bHostTerminationPending = false;
	return true;
}

void USettingModule::FinalizeInterruptedEdit()
{
	if (EditSession.bActive && !IsEditSessionOwner(EditSessionPlayer.Get())) bHostTerminationPending = true;
	if (bHostTerminationPending && !FinalizeHostTermination()) UE_LOG(LogTemp, Error, TEXT("本地玩家设置恢复尚未完成。"));
}

bool USettingModule::RetryHostTerminationRecovery()
{
	return FinalizeHostTermination();
}

void USettingModule::OnTermination(EPhase InPhase)
{
	Super::OnTermination(InPhase);
	if (PHASEC(InPhase, EPhase::Primary))
	{
		ActiveLocalProfile = NAME_None;
		bLocalProfilePhaseInProgress = false;
		bLocalProfilePhaseComplete = false;
		bHostTerminationPending = true;
		if (!bStartingEdit && !bEndingEdit && !bApplyingEdit && !bCancellingEdit && !FinalizeHostTermination())
			UE_LOG(LogTemp, Error, TEXT("世界终止后本地玩家设置恢复失败，保留未完成会话。"));
	}
}

void USettingModule::CollectPropertyDefinitions(const UStruct* InStruct, const FString& InPrefix, TArray<FSettingDefinition>& OutDefinitions) const
{
	for (TFieldIterator<FProperty> Iterator(InStruct); Iterator; ++Iterator)
	{
		const FProperty* Property = *Iterator;
		if (Property->GetOwnerStruct() != InStruct || HasMetadata(*Property, TEXT("SettingHidden")))
		{
			continue;
		}
		if (InPrefix.IsEmpty() && (Property->GetFName() == GET_MEMBER_NAME_CHECKED(FSettingModuleSaveData, AudioData) ||
		                           Property->GetFName() == GET_MEMBER_NAME_CHECKED(FSettingModuleSaveData, ParameterData)))
		{
			continue;
		}

		const FString PropertyPath = InPrefix.IsEmpty() ? Property->GetName() : InPrefix + TEXT(".") + Property->GetName();
		if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
		{
			if (StructProperty->Struct != TBaseStructure<FKey>::Get() && !HasMetadata(*Property, TEXT("SettingLeaf")))
			{
				CollectPropertyDefinitions(StructProperty->Struct, PropertyPath, OutDefinitions);
				continue;
			}
		}

		if (!FSettingPropertyAccess::IsSupported(Property))
		{
			continue;
		}

		FString SettingPath = PropertyPath;
		FString Root;
		FString Remainder;
		if (SettingPath.Split(TEXT("."), &Root, &Remainder) && Root.EndsWith(TEXT("Data")))
		{
			Root.LeftChopInline(4);
			SettingPath = Root + TEXT(".") + Remainder;
		}

		FSettingDefinition& Definition = OutDefinitions.AddDefaulted_GetRef();
		Definition.SettingId.Name = FName(*SettingPath);
		Definition.SourcePath = PropertyPath;
		Definition.Page = HasMetadata(*Property, TEXT("SettingPage")) ? FName(*GetMetadata(*Property, TEXT("SettingPage"))) : FName(*Root);
		if (HasMetadata(*Property, TEXT("SettingCategory")))
		{
			Definition.Category = FName(*GetMetadata(*Property, TEXT("SettingCategory")));
		}
		else if (HasMetadata(*Property, TEXT("Category")))
		{
			Definition.Category = FName(*GetMetadata(*Property, TEXT("Category")));
		}
		else
		{
			Definition.Category = FName(TEXT("General"));
		}
		Definition.Order = HasMetadata(*Property, TEXT("SettingOrder"))
		                       ? FCString::Atoi(*GetMetadata(*Property, TEXT("SettingOrder")))
		                       : OutDefinitions.Num() - 1;
		if (HasMetadata(*Property, TEXT("SettingCategoryDisplayName")))
		{
			Definition.CategoryDisplayName = FText::FromString(GetMetadata(*Property, TEXT("SettingCategoryDisplayName")));
		}
		Definition.CategoryOrder = HasMetadata(*Property, TEXT("SettingCategoryOrder"))
		                               ? FCString::Atoi(*GetMetadata(*Property, TEXT("SettingCategoryOrder")))
		                               : 0;
		if (HasMetadata(*Property, TEXT("DisplayName")))
		{
			Definition.DisplayName = GetPropertyDisplayName(*Property);
		}
		else
		{
			FString DisplayPath = SettingPath;
			DisplayPath.ReplaceInline(TEXT("."), TEXT(" "));
			Definition.DisplayName = FText::FromString(FName::NameToDisplayString(DisplayPath, false));
		}
		Definition.Description = GetPropertyToolTip(*Property);
		Definition.Renderer = FSettingPropertyAccess::InferRenderer(Property);
		const FString Renderer = GetMetadata(*Property, TEXT("SettingRenderer"));
		if (Renderer.Equals(TEXT("Bool"), ESearchCase::IgnoreCase))
			Definition.Renderer = ESettingRendererType::Bool;
		else if (Renderer.Equals(TEXT("Number"), ESearchCase::IgnoreCase))
			Definition.Renderer = ESettingRendererType::Number;
		else if (Renderer.Equals(TEXT("Enum"), ESearchCase::IgnoreCase))
			Definition.Renderer = ESettingRendererType::Enum;
		else if (Renderer.Equals(TEXT("Text"), ESearchCase::IgnoreCase))
			Definition.Renderer = ESettingRendererType::Text;
		else if (Renderer.Equals(TEXT("Option"), ESearchCase::IgnoreCase))
			Definition.Renderer = ESettingRendererType::Option;
		else if (Renderer.Equals(TEXT("Key"), ESearchCase::IgnoreCase))
			Definition.Renderer = ESettingRendererType::Key;
		else if (Renderer.Equals(TEXT("Custom"), ESearchCase::IgnoreCase))
			Definition.Renderer = ESettingRendererType::Custom;

		const FString ApplyPolicy = GetMetadata(*Property, TEXT("SettingApply"));
		Definition.ApplyPolicy = ApplyPolicy.Equals(TEXT("Preview"), ESearchCase::IgnoreCase)     ? ESettingApplyPolicy::Preview
		                         : ApplyPolicy.Equals(TEXT("Immediate"), ESearchCase::IgnoreCase) ? ESettingApplyPolicy::Immediate
		                                                                                          : ESettingApplyPolicy::Deferred;
		Definition.bVisible = !HasMetadata(*Property, TEXT("SettingVisible")) ||
		                      !GetMetadata(*Property, TEXT("SettingVisible")).Equals(TEXT("false"), ESearchCase::IgnoreCase);
		Definition.bEnabled = !HasMetadata(*Property, TEXT("SettingEnabled")) ||
		                      !GetMetadata(*Property, TEXT("SettingEnabled")).Equals(TEXT("false"), ESearchCase::IgnoreCase);
		Definition.bRequiresConfirmation = HasMetadata(*Property, TEXT("SettingRequiresConfirmation"));

		const FString MinMetadata = HasMetadata(*Property, TEXT("UIMin")) ? TEXT("UIMin") : TEXT("ClampMin");
		const FString MaxMetadata = HasMetadata(*Property, TEXT("UIMax")) ? TEXT("UIMax") : TEXT("ClampMax");
		if (HasMetadata(*Property, FName(*MinMetadata)))
		{
			Definition.NumberDisplay.bHasMin = true;
			Definition.NumberDisplay.Min = FCString::Atod(*GetMetadata(*Property, FName(*MinMetadata)));
		}
		if (HasMetadata(*Property, FName(*MaxMetadata)))
		{
			Definition.NumberDisplay.bHasMax = true;
			Definition.NumberDisplay.Max = FCString::Atod(*GetMetadata(*Property, FName(*MaxMetadata)));
		}
		if (HasMetadata(*Property, TEXT("Delta")))
		{
			Definition.NumberDisplay.Step = FCString::Atod(*GetMetadata(*Property, TEXT("Delta")));
		}
		if (HasMetadata(*Property, TEXT("SettingScale")))
		{
			Definition.NumberDisplay.Scale = FCString::Atod(*GetMetadata(*Property, TEXT("SettingScale")));
		}
		if (HasMetadata(*Property, TEXT("SettingDecimalPlaces")))
		{
			Definition.NumberDisplay.DecimalPlaces = FCString::Atoi(*GetMetadata(*Property, TEXT("SettingDecimalPlaces")));
		}
		if (HasMetadata(*Property, TEXT("Units")))
		{
			Definition.NumberDisplay.Suffix = FText::FromString(GetMetadata(*Property, TEXT("Units")));
		}

		if (const FEnumProperty* EnumProperty = CastField<FEnumProperty>(Property))
		{
			Definition.Enum = EnumProperty->GetEnum();
		}
		else if (const FByteProperty* ByteProperty = CastField<FByteProperty>(Property))
		{
			Definition.Enum = ByteProperty->Enum;
		}
		if (Definition.Enum)
		{
			for (int32 Index = 0; Index < Definition.Enum->NumEnums(); ++Index)
			{
				if (IsEnumValueHidden(*Definition.Enum, Index))
				{
					continue;
				}
				const int64 EnumValue = Definition.Enum->GetValueByIndex(Index);
				if (EnumValue == INDEX_NONE || Definition.Enum->GetNameStringByIndex(Index).EndsWith(TEXT("_MAX")))
				{
					continue;
				}

				FSettingOption& Option = Definition.Options.AddDefaulted_GetRef();
				Option.Value = FParameter(EnumValue);
				Option.DisplayName = GetEnumValueDisplayName(*Definition.Enum, Index);
			}
		}
	}
}

void USettingModule::EnsureBuiltinProviders()
{
	EnsureSettingProvider<UCameraSettingProvider>(this, Providers);
	EnsureSettingProvider<UAudioSettingProvider>(this, Providers);
	EnsureSettingProvider<UParameterSettingProvider>(this, Providers);
	EnsureSettingProvider<UInputSettingProvider>(this, Providers);
	EnsureSettingProvider<UResolutionSettingProvider>(this, Providers);
	EnsureSettingProvider<ULanguageSettingProvider>(this, Providers);
}

void USettingModule::BuildSettingDefinitions()
{
	EnsureBuiltinProviders();

	FinalDefinitions.Reset();
	CollectPropertyDefinitions(FSettingModuleSaveData::StaticStruct(), FString(), FinalDefinitions);
	for (const USettingProviderBase* Provider : Providers)
	{
		if (Provider)
		{
			Provider->CollectDefinitions(FinalDefinitions);
		}
	}

	TMap<FSettingId, int32> DefinitionIndexMap;
	TArray<FSettingDefinition> DeduplicatedDefinitions;
	for (FSettingDefinition& Definition : FinalDefinitions)
	{
		if (int32* ExistingIndex = DefinitionIndexMap.Find(Definition.SettingId))
		{
			DeduplicatedDefinitions[*ExistingIndex] = MoveTemp(Definition);
		}
		else
		{
			const int32 Index = DeduplicatedDefinitions.Add(MoveTemp(Definition));
			DefinitionIndexMap.Add(DeduplicatedDefinitions[Index].SettingId, Index);
		}
	}
	FinalDefinitions = MoveTemp(DeduplicatedDefinitions);

	for (FSettingDefinition& Definition : FinalDefinitions)
	{
		const FSettingDefinitionOverride* Override = SettingConfig.Overrides.FindByPredicate(
		    [&Definition](const FSettingDefinitionOverride& Item)
		    {
			    return Item.SettingId == Definition.SettingId;
		    });
		if (!Override)
		{
			continue;
		}
		if (Override->bOverrideDisplayName)
			Definition.DisplayName = Override->DisplayName;
		if (Override->bOverrideDescription)
			Definition.Description = Override->Description;
		if (Override->bOverridePage)
			Definition.Page = Override->Page;
		if (Override->bOverrideCategory)
			Definition.Category = Override->Category;
		if (Override->bOverrideCategoryOrder)
			Definition.CategoryOrder = Override->CategoryOrder;
		if (Override->bOverrideOrder)
			Definition.Order = Override->Order;
		if (Override->bOverrideRenderer)
			Definition.Renderer = Override->Renderer;
		if (Override->bOverrideCustomWidgetClass)
			Definition.CustomWidgetClass = Override->CustomWidgetClass;
		if (Override->bOverrideApplyPolicy)
			Definition.ApplyPolicy = Override->ApplyPolicy;
		if (Override->bOverrideVisible)
			Definition.bVisible = Override->bVisible;
		if (Override->bOverrideEnabled)
			Definition.bEnabled = Override->bEnabled;
		if (Override->bOverrideRequiresConfirmation)
			Definition.bRequiresConfirmation = Override->bRequiresConfirmation;
		if (Override->bOverrideNumberDisplay)
			Definition.NumberDisplay = Override->NumberDisplay;
		if (Override->bOverrideVisibleConditions)
			Definition.VisibleConditions = Override->VisibleConditions;
		if (Override->bOverrideEnableConditions)
			Definition.EnableConditions = Override->EnableConditions;
	}

	TSet<FSettingId> UniqueIds;
	for (const FSettingDefinition& Definition : FinalDefinitions)
	{
		ensureEditorMsgf(Definition.SettingId.IsValid() && !UniqueIds.Contains(Definition.SettingId),
		                 FString::Printf(TEXT("Duplicate or invalid setting id: %s"), *Definition.SettingId.Name.ToString()),
		                 EDC_Default,
		                 EDV_Error);
		UniqueIds.Add(Definition.SettingId);
	}
	FSettingModuleSaveData ValidationData;
	for (const FSettingDefinition& Definition : FinalDefinitions)
	{
		ensureEditorMsgf(
		    !Definition.Page.IsNone(), FString::Printf(TEXT("Setting %s has no page."), *Definition.SettingId.Name.ToString()), EDC_Default, EDV_Error);
		if (!FindSettingProvider(Definition))
		{
			FResolvedSettingProperty Resolved;
			ensureEditorMsgf(FSettingPropertyAccess::Resolve(FSettingModuleSaveData::StaticStruct(), &ValidationData, Definition.SourcePath, Resolved),
			                 FString::Printf(TEXT("Setting %s has invalid source path %s."), *Definition.SettingId.Name.ToString(), *Definition.SourcePath),
			                 EDC_Default,
			                 EDV_Error);
		}

		for (int32 OptionIndex = 0; OptionIndex < Definition.Options.Num(); ++OptionIndex)
		{
			ensureEditorMsgf(!Definition.Options.ContainsByPredicate(
			                     [&Definition, OptionIndex](const FSettingOption& Option)
			                     {
				                     return &Option != &Definition.Options[OptionIndex] && Option.Value == Definition.Options[OptionIndex].Value;
			                     }),
			                 FString::Printf(TEXT("Setting %s has duplicate option values."), *Definition.SettingId.Name.ToString()),
			                 EDC_Default,
			                 EDV_Error);
		}
		for (const FSettingCondition& Condition : Definition.VisibleConditions)
		{
			ensureEditorMsgf(UniqueIds.Contains(Condition.OtherSetting),
			                 FString::Printf(TEXT("Setting %s references missing visible condition %s."),
			                                 *Definition.SettingId.Name.ToString(),
			                                 *Condition.OtherSetting.Name.ToString()),
			                 EDC_Default,
			                 EDV_Error);
		}
		for (const FSettingCondition& Condition : Definition.EnableConditions)
		{
			ensureEditorMsgf(UniqueIds.Contains(Condition.OtherSetting),
			                 FString::Printf(TEXT("Setting %s references missing enable condition %s."),
			                                 *Definition.SettingId.Name.ToString(),
			                                 *Condition.OtherSetting.Name.ToString()),
			                 EDC_Default,
			                 EDV_Error);
		}
	}

	FinalDefinitions.Sort(
	    [](const FSettingDefinition& A, const FSettingDefinition& B)
	    {
		    if (A.Page != B.Page)
			    return A.Page.LexicalLess(B.Page);
		    if (A.CategoryOrder != B.CategoryOrder)
			    return A.CategoryOrder < B.CategoryOrder;
		    if (A.Category != B.Category)
			    return A.Category.LexicalLess(B.Category);
		    return A.Order < B.Order;
	    });

	if (EditSession.bActive)
	{
		BuildSettingEntries();
	}
}

const FSettingDefinition* USettingModule::FindSettingDefinition(FSettingId InSettingId) const
{
	return FinalDefinitions.FindByPredicate(
	    [InSettingId](const FSettingDefinition& Definition)
	    {
		    return Definition.SettingId == InSettingId;
	    });
}

USettingProviderBase* USettingModule::FindSettingProvider(const FSettingDefinition& InDefinition) const
{
	for (USettingProviderBase* Provider : Providers)
	{
		if (Provider && Provider->CanHandle(InDefinition))
		{
			return Provider;
		}
	}
	return nullptr;
}

FSettingValidationResult USettingModule::ValidateSettingValue(const FSettingDefinition& InDefinition, const FParameter& InValue) const
{
	if (!InValue.HasValue())
	{
		return FSettingValidationResult::Invalid(NSLOCTEXT("WH.SettingModule", "MissingValue", "设置值不能为空。"));
	}

	const FParameter CurrentValue = GetPendingValue(InDefinition.SettingId);
	if (CurrentValue.HasValue() && CurrentValue.GetValueStruct() != InValue.GetValueStruct())
	{
		return FSettingValidationResult::Invalid(NSLOCTEXT("WH.SettingModule", "InvalidType", "设置值类型不匹配。"));
	}

	if (!InDefinition.Options.IsEmpty() && !InDefinition.Options.ContainsByPredicate(
	                                           [&InValue](const FSettingOption& Option)
	                                           {
		                                           return Option.Value == InValue;
	                                           }))
	{
		return FSettingValidationResult::Invalid(NSLOCTEXT("WH.SettingModule", "InvalidOption", "该值不在可选项中。"));
	}

	double NumericValue = 0.0;
	if (TryGetNumericValue(InValue, NumericValue))
	{
		if (!FMath::IsFinite(NumericValue))
		{
			return FSettingValidationResult::Invalid(NSLOCTEXT("WH.SettingModule", "NonFinite", "设置值必须为有限数值。"));
		}
		if (InDefinition.NumberDisplay.bHasMin && NumericValue < InDefinition.NumberDisplay.Min)
		{
			return FSettingValidationResult::Invalid(
			    FText::Format(NSLOCTEXT("WH.SettingModule", "BelowMinimum", "设置值不能小于 {0}。"), FText::AsNumber(InDefinition.NumberDisplay.Min)));
		}
		if (InDefinition.NumberDisplay.bHasMax && NumericValue > InDefinition.NumberDisplay.Max)
		{
			return FSettingValidationResult::Invalid(
			    FText::Format(NSLOCTEXT("WH.SettingModule", "AboveMaximum", "设置值不能大于 {0}。"), FText::AsNumber(InDefinition.NumberDisplay.Max)));
		}
	}

	if (const USettingProviderBase* Provider = FindSettingProvider(InDefinition))
	{
		return Provider->Validate(InDefinition, InValue);
	}
	return FSettingValidationResult::Valid();
}

bool USettingModule::EvaluateConditions(const TArray<FSettingCondition>& InConditions) const
{
	for (const FSettingCondition& Condition : InConditions)
	{
		const FParameter OtherValue = GetPendingValue(Condition.OtherSetting);
		bool bPassed = false;
		switch (Condition.Op)
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
				if (TryGetNumericValue(OtherValue, Left) && TryGetNumericValue(Condition.Value, Right))
				{
					bPassed = Condition.Op == ESettingConditionOp::Greater ? Left > Right : Left < Right;
				}
				break;
			}
		}
		if (!bPassed)
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
	return Definition && FSettingPropertyAccess::Resolve(FSettingModuleSaveData::StaticStruct(), &InData, Definition->SourcePath, Resolved) ? FSettingPropertyAccess::Read(Resolved)
	                                                                                                                            : FParameter();
}

bool USettingModule::WriteSessionValue(FSettingModuleSaveData& InData, FSettingId InSettingId, const FParameter& InValue) const
{
	const FSettingDefinition* Definition = FindSettingDefinition(InSettingId);
	FResolvedSettingProperty Resolved;
	return Definition && FSettingPropertyAccess::Resolve(FSettingModuleSaveData::StaticStruct(), &InData, Definition->SourcePath, Resolved) &&
	       FSettingPropertyAccess::Write(Resolved, InValue);
}

bool USettingModule::GetCurrentCombinedSettings(FSettingModuleSaveData& OutData) const
{
	UWidgetModule* Widget = UWidgetModule::GetPtr(GetWorld());
	UAudioModule* Audio = UAudioModule::GetPtr(GetWorld());
	UVideoModule* Video = UVideoModule::GetPtr(GetWorld());
	UCameraModule* Camera = UCameraModule::GetPtr(GetWorld());
	UInputModule* Input = UInputModule::GetPtr(GetWorld());
	UParameterModule* Parameters = UParameterModule::GetPtr(GetWorld());
	if (!Widget || !Audio || !Video || !Camera || !Input || !Parameters)
	{
		UE_LOG(LogTemp, Error, TEXT("无法读取当前世界的完整设置：必要模块不可用。"));
		return false;
	}
	const FParameter WidgetData = Widget->GetSaveData(true);
	const FParameter AudioData = Audio->GetSaveData(true);
	const FParameter VideoData = Video->GetSaveData(true);
	const FParameter CameraData = Camera->GetSaveData(true);
	const FParameter InputData = Input->GetSaveData(false);
	const FParameter ParameterData = Parameters->GetSaveData(true);
	if (!WidgetData.Is<FWidgetModuleSaveData>() || !AudioData.Is<FAudioModuleSaveData>() ||
		!VideoData.Is<FVideoModuleSaveData>() || !CameraData.Is<FCameraModuleSaveData>() || !InputData.Is<FInputModuleSaveData>() ||
		!ParameterData.Is<FParameterModuleSaveData>())
	{
		UE_LOG(LogTemp, Error, TEXT("无法读取当前世界的完整设置：模块返回的数据无效。"));
		return false;
	}
	OutData.WidgetData = WidgetData.GetRef<FWidgetModuleSaveData>();
	OutData.AudioData = AudioData.GetRef<FAudioModuleSaveData>();
	OutData.VideoData = VideoData.GetRef<FVideoModuleSaveData>();
	OutData.CameraData = CameraData.GetRef<FCameraModuleSaveData>();
	OutData.InputData = InputData.GetRef<FInputModuleSaveData>();
	OutData.ParameterData = ParameterData.GetRef<FParameterModuleSaveData>();
	return true;
}

FSettingModuleSaveData USettingModule::GetDefaultCombinedSettings() const
{
	return FSettingModuleSaveData();
}

bool USettingModule::ApplyCombinedSettings(FSettingModuleSaveData& InData)
{
	if (!IsBusinessAdmissionOpen()) return false;
	UWidgetModule* Widget = UWidgetModule::GetPtr(GetWorld());
	UVideoModule* Video = UVideoModule::GetPtr(GetWorld());
	if (!Widget || !Video)
	{
		UE_LOG(LogTemp, Error, TEXT("无法应用当前世界的设置：必要模块不可用。"));
		return false;
	}
	if (!Widget->LoadSaveData(FParameter(InData.WidgetData), EPhase::All)) return false;
	return Video->LoadSaveData(FParameter(InData.VideoData), EPhase::All);
}

bool USettingModule::SaveSettings()
{
	if (!IsBusinessAdmissionOpen()) return false;
	USaveGameModule* Saves = USaveGameModule::GetPtr(GetWorld());
	const bool bSaved = Saves && !ActiveLocalProfile.IsNone() && Saves->SaveProfile(ActiveLocalProfile).bSuccess;
	bLastSettingsSaveSucceeded = bSaved;
	return bLastSettingsSaveSucceeded;
}

bool USettingModule::PrepareLocalProfileLoad(FName InProfileName)
{
	if (!IsBusinessAdmissionOpen()) return false;
	UWorld* World = GetWorld();
	if (InProfileName.IsNone() || !World) return false;
	UInputModule* Input = UInputModule::GetPtr(World);
	UCameraModule* Camera = UCameraModule::GetPtr(World);
	if (!Input || !Camera || !UWidgetModule::GetPtr(World) || !UAudioModule::GetPtr(World) ||
		!UVideoModule::GetPtr(World) || !UParameterModule::GetPtr(World)) return false;
	if (EditSession.bActive) EndEdit();
	if (EditSession.bActive || !Input->PrepareLocalProfileLoad(InProfileName)) return false;
	if (!Camera->PrepareLocalProfileLoad(InProfileName)) return false;
	ActiveLocalProfile = InProfileName;
	bLocalProfilePhaseComplete = false;
	return true;
}

bool USettingModule::IsLocalProfileDataSupported(const FParameter& InData) const
{
	const FSettingModuleSaveData* Data = InData.GetPtr<FSettingModuleSaveData>();
	const UCameraModule* Camera = UCameraModule::GetPtr(GetWorld());
	return Data && (!Camera || Camera->ValidateLocalProfileData(Data->CameraData));
}

bool USettingModule::LoadLocalProfilePhase(const FParameter& InData, EPhase InPhase)
{
	if (!IsBusinessAdmissionOpen()) return false;
	if (bLocalProfilePhaseInProgress || ActiveLocalProfile.IsNone() || !IsLocalProfileDataSupported(InData)) return false;
	TGuardValue<bool> Loading(bLocalProfilePhaseInProgress, true);
	bLocalProfilePhaseComplete = false;
	if (!LoadSaveData(InData, InPhase)) return false;
	if (!bLocalProfilePhaseComplete) return false;
	if (!PHASEC(InPhase, EPhase::Final)) return true;
	const UInputModule* Input = UInputModule::GetPtr(GetWorld());
	const UCameraModule* Camera = UCameraModule::GetPtr(GetWorld());
	return Input && Camera && Input->AreLocalProfilesApplied(ActiveLocalProfile) && Camera->AreLocalProfilesApplied(ActiveLocalProfile);
}

void USettingModule::MarkLocalProfilePhaseComplete()
{
	if (bLocalProfilePhaseInProgress) bLocalProfilePhaseComplete = true;
}

void USettingModule::AbortLocalProfileLoad()
{
	ActiveLocalProfile = NAME_None;
	bLocalProfilePhaseComplete = false;
}

bool USettingModule::CanSaveLocalProfile(FName InProfileName) const
{
	const UInputModule* Input = UInputModule::GetPtr(GetWorld());
	const UCameraModule* Camera = UCameraModule::GetPtr(GetWorld());
	return !InProfileName.IsNone() && ActiveLocalProfile == InProfileName && Input && Camera &&
		Input->AreLocalProfilesApplied(InProfileName) && Camera->AreLocalProfilesApplied(InProfileName);
}

void USettingModule::BuildSettingEntries()
{
	SettingEntries.Reset();
	SettingEntryMap.Reset();
	for (const FSettingDefinition& Definition : FinalDefinitions)
	{
		USettingEntry* Entry = NewObject<USettingEntry>(this);
		Entry->Initialize(this, Definition);
		SettingEntries.Add(Entry);
		SettingEntryMap.Add(Definition.SettingId, Entry);
	}
}

void USettingModule::RefreshSettingDefinitions()
{
	if (!IsBusinessAdmissionOpen()) return;
	BuildSettingDefinitions();
}

bool USettingModule::BeginEdit(APlayerController* InPlayer)
{
	if (!IsBusinessAdmissionOpen()) return false;
	if (bStartingEdit || bEndingEdit || EditSession.bActive || !::IsValid(InPlayer) || !InPlayer->IsLocalController() || InPlayer->GetWorld() != GetWorld())
	{
		return false;
	}
	ON_SCOPE_EXIT { FinalizeInterruptedEdit(); };
	TGuardValue<bool> Starting(bStartingEdit, true);
	BuildSettingDefinitions();

	FSettingModuleSaveData AppliedData;
	if (!GetCurrentCombinedSettings(AppliedData)) return false;
	UWHLocalPlayer* Local = Cast<UWHLocalPlayer>(InPlayer->GetLocalPlayer());
	uint64 Generation = 0;
	if (!Local || !Local->AcquireSettingEdit(this, Generation)) return false;
	EditSessionLocalPlayer = Local;
	EditSessionGeneration = Generation;
	EditSession.AppliedData = MoveTemp(AppliedData);
	EditSessionPlayer = InPlayer;
	EditSession.PendingData = EditSession.AppliedData;
	EditSession.DefaultData = GetDefaultCombinedSettings();
	EditSession.bActive = true;
	bEndEditRequested = false;
	AppliedUncommittedSettings.Reset();
	ConfirmationTransaction = FSettingConfirmationTransaction();
	ValidationResults.Reset();
	const TArray<FSettingDefinition> DefinitionSnapshot = FinalDefinitions;
	for (USettingProviderBase* Provider : Providers)
	{
		if (Provider) Provider->BeginEdit(DefinitionSnapshot);
	}
	BuildSettingEntries();
	BroadcastEditStateChanged();
	return true;
}

bool USettingModule::IsEditSessionOwner(APlayerController* InPlayer) const
{
	const UWHLocalPlayer* Local = EditSessionLocalPlayer.Get();
	return EditSession.bActive && ::IsValid(InPlayer) && EditSessionPlayer.Get() == InPlayer && InPlayer->GetWorld() == GetWorld() &&
		::IsValid(Local) && InPlayer->GetLocalPlayer() == Local && Local->IsSettingEditCurrent(this, EditSessionGeneration);
}

bool USettingModule::EndEdit()
{
	if (!IsBusinessAdmissionOpen()) return false;
	if (!EditSession.bActive)
	{
		bEndEditRequested = false;
		return true;
	}
	if (!IsEditSessionOwner(EditSessionPlayer.Get()))
	{
		bHostTerminationPending = true;
		return FinalizeHostTermination();
	}
	if (bEndingEdit) return false;
	if (bStartingEdit || bCancellingEdit || bApplyingEdit)
	{
		bEndEditRequested = true;
		return false;
	}
	const bool bAlreadyRequested = bEndEditRequested;
	bEndEditRequested = true;
	ON_SCOPE_EXIT { FinalizeInterruptedEdit(); };
	TGuardValue<bool> Ending(bEndingEdit, true);
	if (!UWidgetModule::GetPtr(GetWorld()) || !UVideoModule::GetPtr(GetWorld()) || !CancelEditSession())
	{
		if (!bAlreadyRequested) UE_LOG(LogTemp, Error, TEXT("设置会话尚未关闭：恢复未提交的设置失败。"));
		return false;
	}
	for (USettingProviderBase* Provider : Providers)
	{
		if (Provider) Provider->EndEdit();
	}
	if (UWHLocalPlayer* Local = EditSessionLocalPlayer.Get()) Local->ReleaseSettingEdit(this, EditSessionGeneration);
	EditSessionLocalPlayer.Reset();
	EditSessionGeneration = 0;
	EditSession = FSettingEditSession();
	EditSessionPlayer.Reset();
	SettingEntries.Reset();
	SettingEntryMap.Reset();
	ValidationResults.Reset();
	bEndEditRequested = false;
	BroadcastEditStateChanged();
	return true;
}

bool USettingModule::ApplyEditSession()
{
	if (!IsBusinessAdmissionOpen()) return false;
	if (!IsEditSessionOwner(EditSessionPlayer.Get()) || ConfirmationTransaction.bActive || bEndEditRequested || bCancellingEdit || bApplyingEdit)
	{
		return false;
	}
	ON_SCOPE_EXIT { FinalizeInterruptedEdit(); };
	TGuardValue<bool> Applying(bApplyingEdit, true);

	TArray<FSettingDefinition> DirtyDefinitions;
	bool bRequiresConfirmation = false;
	const TArray<FSettingDefinition> Definitions = FinalDefinitions;
	for (const FSettingDefinition& Definition : Definitions)
	{
		const bool bDirty = IsSettingDirty(Definition.SettingId);
		if (bEndEditRequested) return false;
		if (!bDirty)
		{
			continue;
		}
		const FSettingValidationResult Validation = ValidateSettingValue(Definition, GetPendingValue(Definition.SettingId));
		if (bEndEditRequested) return false;
		ValidationResults.Add(Definition.SettingId, Validation);
		if (!Validation.bValid)
		{
			BroadcastEditStateChanged();
			return false;
		}
		DirtyDefinitions.Add(Definition);
		bRequiresConfirmation |= Definition.bRequiresConfirmation;
	}
	if (DirtyDefinitions.IsEmpty())
	{
		return false;
	}

	if (!ApplyCombinedSettings(EditSession.PendingData)) return false;
	if (bEndEditRequested) return false;
	for (const FSettingDefinition& Definition : DirtyDefinitions)
	{
		if (USettingProviderBase* Provider = FindSettingProvider(Definition))
		{
			AppliedUncommittedSettings.Add(Definition.SettingId);
			const bool bApplied = Provider->Apply(Definition, GetPendingValue(Definition.SettingId));
			if (bEndEditRequested) return false;
			if (!bApplied)
			{
				const bool bCombinedRestored = ApplyCombinedSettings(EditSession.AppliedData);
				const bool bProvidersRestored = RestorePendingProviders();
				if (!bCombinedRestored || !bProvidersRestored)
					UE_LOG(LogTemp, Error, TEXT("设置应用失败，部分预览未能恢复，编辑会话保留。"));
				return false;
			}
		}
	}

	if (bRequiresConfirmation)
	{
		ConfirmationTransaction.PreviousData = EditSession.AppliedData;
		ConfirmationTransaction.SettingIds.Reset();
		for (const FSettingDefinition& Definition : DirtyDefinitions)
		{
			if (Definition.bRequiresConfirmation)
			{
				ConfirmationTransaction.SettingIds.Add(Definition.SettingId);
			}
		}
		ConfirmationTransaction.ExpiresAt = FPlatformTime::Seconds() + ConfirmationTimeout;
		ConfirmationTransaction.bActive = true;
		BroadcastEditStateChanged();
		return true;
	}

	EditSession.AppliedData = EditSession.PendingData;
	for (USettingProviderBase* Provider : Providers)
	{
		if (Provider) Provider->Commit();
	}
	AppliedUncommittedSettings.Reset();
	SaveSettings();
	ValidationResults.Reset();
	BroadcastEditStateChanged();
	return true;
}

bool USettingModule::RestorePendingProviders()
{
	const TArray<FSettingDefinition> Definitions = FinalDefinitions;
	for (const FSettingDefinition& Definition : Definitions)
	{
		if (!IsEditSessionOwner(EditSessionPlayer.Get())) return false;
		USettingProviderBase* Provider = FindSettingProvider(Definition);
		if (!Provider) continue;
		if (AppliedUncommittedSettings.Contains(Definition.SettingId))
		{
			if (!Provider->Rollback(Definition)) return false;
			AppliedUncommittedSettings.Remove(Definition.SettingId);
		}
		else if (Provider->GetPendingValue(Definition) != Provider->GetAppliedValue(Definition) &&
			!Provider->SetPendingValue(Definition, Provider->GetAppliedValue(Definition))) return false;
	}
	return AppliedUncommittedSettings.IsEmpty();
}

bool USettingModule::CancelEditSession()
{
	if (!IsBusinessAdmissionOpen()) return false;
	if (!EditSession.bActive)
	{
		return true;
	}
	if (!IsEditSessionOwner(EditSessionPlayer.Get())) return false;
	if (ConfirmationTransaction.bActive)
	{
		return RejectPendingSettings();
	}
	if (bCancellingEdit || bApplyingEdit) return false;
	ON_SCOPE_EXIT { FinalizeInterruptedEdit(); };
	TGuardValue<bool> Cancelling(bCancellingEdit, true);
	if (!ApplyCombinedSettings(EditSession.AppliedData) || !RestorePendingProviders()) return false;
	EditSession.PendingData = EditSession.AppliedData;
	ValidationResults.Reset();
	const TArray<FSettingDefinition> Definitions = FinalDefinitions;
	for (const FSettingDefinition& Definition : Definitions)
	{
		OnSettingValueChanged.Broadcast(Definition.SettingId);
	}
	BroadcastEditStateChanged();
	return true;
}

double USettingModule::GetConfirmationTimeRemaining() const
{
	return ConfirmationTransaction.bActive ? FMath::Max(0.0, ConfirmationTransaction.ExpiresAt - FPlatformTime::Seconds()) : 0.0;
}

bool USettingModule::ConfirmPendingSettings()
{
	if (!IsBusinessAdmissionOpen()) return false;
	if (!IsEditSessionOwner(EditSessionPlayer.Get()) || !ConfirmationTransaction.bActive || bEndEditRequested || bCancellingEdit || bApplyingEdit)
	{
		return false;
	}
	ON_SCOPE_EXIT { FinalizeInterruptedEdit(); };
	TGuardValue<bool> Applying(bApplyingEdit, true);

	EditSession.AppliedData = EditSession.PendingData;
	for (USettingProviderBase* Provider : Providers)
	{
		if (Provider) Provider->Commit();
	}
	ConfirmationTransaction = FSettingConfirmationTransaction();
	ValidationResults.Reset();
	AppliedUncommittedSettings.Reset();
	SaveSettings();
	BroadcastEditStateChanged();
	return true;
}

bool USettingModule::RejectPendingSettings()
{
	if (!IsBusinessAdmissionOpen()) return false;
	if (!IsEditSessionOwner(EditSessionPlayer.Get()) || !ConfirmationTransaction.bActive || bCancellingEdit || bApplyingEdit)
	{
		return false;
	}
	ON_SCOPE_EXIT { FinalizeInterruptedEdit(); };
	TGuardValue<bool> Cancelling(bCancellingEdit, true);
	if (!ApplyCombinedSettings(ConfirmationTransaction.PreviousData) || !RestorePendingProviders()) return false;
	EditSession.PendingData = EditSession.AppliedData;
	ConfirmationTransaction = FSettingConfirmationTransaction();
	ValidationResults.Reset();
	const TArray<FSettingDefinition> Definitions = FinalDefinitions;
	for (const FSettingDefinition& Definition : Definitions)
	{
		OnSettingValueChanged.Broadcast(Definition.SettingId);
	}
	BroadcastEditStateChanged();
	return true;
}

void USettingModule::ResetAllToDefault()
{
	if (!IsBusinessAdmissionOpen()) return;
	if (!EditSession.bActive)
	{
		return;
	}
	for (const FSettingDefinition& Definition : FinalDefinitions)
	{
		SetPendingValue(Definition.SettingId, GetDefaultValue(Definition.SettingId));
	}
	BroadcastEditStateChanged();
}

bool USettingModule::CanApply() const
{
	if (!IsBusinessAdmissionOpen()) return false;
	if (!EditSession.bActive || ConfirmationTransaction.bActive)
		return false;
	for (const FSettingDefinition& Definition : FinalDefinitions)
	{
		if (IsSettingDirty(Definition.SettingId))
			return true;
	}
	return false;
}

bool USettingModule::CanReset() const
{
	if (!IsBusinessAdmissionOpen()) return false;
	if (!EditSession.bActive)
		return false;
	for (const FSettingDefinition& Definition : FinalDefinitions)
	{
		if (CanResetSetting(Definition.SettingId))
			return true;
	}
	return false;
}

bool USettingModule::IsSettingDirty(FSettingId InSettingId) const
{
	if (!EditSession.bActive)
		return false;
	return GetAppliedValue(InSettingId) != GetPendingValue(InSettingId);
}

bool USettingModule::CanResetSetting(FSettingId InSettingId) const
{
	if (!IsBusinessAdmissionOpen()) return false;
	if (!EditSession.bActive)
		return false;
	return GetPendingValue(InSettingId) != GetDefaultValue(InSettingId);
}

bool USettingModule::IsSettingEnabled(FSettingId InSettingId) const
{
	if (const FSettingDefinition* Definition = FindSettingDefinition(InSettingId))
	{
		return Definition->bEnabled && EvaluateConditions(Definition->EnableConditions);
	}
	return false;
}

bool USettingModule::IsSettingVisible(FSettingId InSettingId) const
{
	if (const FSettingDefinition* Definition = FindSettingDefinition(InSettingId))
	{
		return Definition->bVisible && EvaluateConditions(Definition->VisibleConditions);
	}
	return false;
}

FSettingValidationResult USettingModule::GetValidationResult(FSettingId InSettingId) const
{
	if (const FSettingValidationResult* Result = ValidationResults.Find(InSettingId))
	{
		return *Result;
	}
	return FSettingValidationResult::Valid();
}

FParameter USettingModule::GetAppliedValue(FSettingId InSettingId) const
{
	if (!EditSession.bActive)
		return FParameter();
	if (const FSettingDefinition* Definition = FindSettingDefinition(InSettingId))
	{
		if (const USettingProviderBase* Provider = FindSettingProvider(*Definition))
		{
			return Provider->GetAppliedValue(*Definition);
		}
	}
	return const_cast<USettingModule*>(this)->ReadSessionValue(const_cast<FSettingModuleSaveData&>(EditSession.AppliedData), InSettingId);
}

FParameter USettingModule::GetPendingValue(FSettingId InSettingId) const
{
	if (!EditSession.bActive)
		return FParameter();
	if (const FSettingDefinition* Definition = FindSettingDefinition(InSettingId))
	{
		if (const USettingProviderBase* Provider = FindSettingProvider(*Definition))
		{
			return Provider->GetPendingValue(*Definition);
		}
	}
	return const_cast<USettingModule*>(this)->ReadSessionValue(const_cast<FSettingModuleSaveData&>(EditSession.PendingData), InSettingId);
}

FParameter USettingModule::GetDefaultValue(FSettingId InSettingId) const
{
	if (!EditSession.bActive)
	{
		return FParameter();
	}
	if (const FSettingDefinition* Definition = FindSettingDefinition(InSettingId))
	{
		if (const USettingProviderBase* Provider = FindSettingProvider(*Definition))
		{
			return Provider->GetDefaultValue(*Definition);
		}
	}
	return const_cast<USettingModule*>(this)->ReadSessionValue(const_cast<FSettingModuleSaveData&>(EditSession.DefaultData), InSettingId);
}

bool USettingModule::SetPendingValue(FSettingId InSettingId, const FParameter& InValue)
{
	if (!IsBusinessAdmissionOpen()) return false;
	if (!IsEditSessionOwner(EditSessionPlayer.Get()) || ConfirmationTransaction.bActive || bEndEditRequested || bCancellingEdit || bApplyingEdit)
	{
		return false;
	}
	ON_SCOPE_EXIT { FinalizeInterruptedEdit(); };
	TGuardValue<bool> Applying(bApplyingEdit, true);
	const FSettingDefinition* FoundDefinition = FindSettingDefinition(InSettingId);
	if (!FoundDefinition)
	{
		return false;
	}
	const FSettingDefinition DefinitionCopy = *FoundDefinition;
	const FSettingDefinition* Definition = &DefinitionCopy;
	if (!IsSettingEnabled(InSettingId) || bEndEditRequested) return false;

	const FSettingValidationResult Validation = ValidateSettingValue(*Definition, InValue);
	if (bEndEditRequested) return false;
	ValidationResults.Add(InSettingId, Validation);
	if (!Validation.bValid)
	{
		BroadcastEditStateChanged();
		return false;
	}

	if (USettingProviderBase* Provider = FindSettingProvider(*Definition))
	{
		const FParameter PreviousValue = Provider->GetPendingValue(*Definition);
		if (!Provider->SetPendingValue(*Definition, InValue))
		{
			return false;
		}
		if (bEndEditRequested) return false;
		if (Definition->ApplyPolicy != ESettingApplyPolicy::Deferred)
		{
			AppliedUncommittedSettings.Add(InSettingId);
			const bool bApplied = Definition->ApplyPolicy == ESettingApplyPolicy::Preview ?
				Provider->Preview(*Definition, InValue) : Provider->Apply(*Definition, InValue);
			if (bEndEditRequested) return false;
			if (!bApplied)
			{
				Provider->SetPendingValue(*Definition, PreviousValue);
				const bool bRestored = Provider->Apply(*Definition, PreviousValue);
				if (bEndEditRequested) return false;
				if (bRestored && PreviousValue == Provider->GetAppliedValue(*Definition)) AppliedUncommittedSettings.Remove(InSettingId);
				ValidationResults.Add(InSettingId, FSettingValidationResult::Invalid(bRestored ?
					NSLOCTEXT("WH.SettingModule", "PreviewFailed", "设置预览失败，已恢复原值。") :
					NSLOCTEXT("WH.SettingModule", "PreviewRestoreFailed", "设置预览失败，恢复原值也未完成。")));
				BroadcastEditStateChanged();
				return false;
			}
		}
		OnSettingValueChanged.Broadcast(InSettingId);
		if (bEndEditRequested) return false;
		BroadcastEditStateChanged();
		return true;
	}

	if (Definition->ApplyPolicy != ESettingApplyPolicy::Deferred && !Definition->SourcePath.StartsWith(TEXT("WidgetData.")))
	{
		ValidationResults.Add(InSettingId, FSettingValidationResult::Invalid(NSLOCTEXT("WH.SettingModule", "MissingPreviewProvider", "此设置尚未配置预览接口。")));
		BroadcastEditStateChanged();
		return false;
	}
	const FParameter PreviousValue = GetPendingValue(InSettingId);
	if (!WriteSessionValue(EditSession.PendingData, InSettingId, InValue))
	{
		return false;
	}
	if (Definition->ApplyPolicy != ESettingApplyPolicy::Deferred)
	{
		if (Definition->SourcePath.StartsWith(TEXT("WidgetData.")))
		{
			UWidgetModule* Widgets = UWidgetModule::GetPtr(GetWorld());
			if (!Widgets)
			{
				WriteSessionValue(EditSession.PendingData, InSettingId, PreviousValue);
				ValidationResults.Add(InSettingId, FSettingValidationResult::Invalid(NSLOCTEXT("WH.SettingModule", "WidgetServiceUnavailable", "当前世界的界面设置服务不可用。")));
				BroadcastEditStateChanged();
				return false;
			}
			const FParameter WidgetData = Widgets->GetSaveData(true);
			if (bEndEditRequested) return false;
			if (!WidgetData.Is<FWidgetModuleSaveData>())
			{
				WriteSessionValue(EditSession.PendingData, InSettingId, PreviousValue);
				ValidationResults.Add(InSettingId, FSettingValidationResult::Invalid(NSLOCTEXT("WH.SettingModule", "WidgetPreviewDataUnavailable", "无法读取当前世界的界面设置。")));
				BroadcastEditStateChanged();
				return false;
			}
			FWidgetModuleSaveData Preview = WidgetData.GetRef<FWidgetModuleSaveData>();
			FResolvedSettingProperty Resolved;
			if (!FSettingPropertyAccess::Resolve(FWidgetModuleSaveData::StaticStruct(), &Preview, Definition->SourcePath.RightChop(11), Resolved) || !FSettingPropertyAccess::Write(Resolved, InValue))
			{
				WriteSessionValue(EditSession.PendingData, InSettingId, PreviousValue);
				ValidationResults.Add(InSettingId, FSettingValidationResult::Invalid(NSLOCTEXT("WH.SettingModule", "WidgetPreviewRejected", "界面设置预览未能应用，已恢复原值。")));
				BroadcastEditStateChanged();
				return false;
			}
			if (!Widgets->LoadSaveData(FParameter(Preview), EPhase::All))
			{
				WriteSessionValue(EditSession.PendingData, InSettingId, PreviousValue);
				ValidationResults.Add(InSettingId, FSettingValidationResult::Invalid(NSLOCTEXT("WH.SettingModule", "WidgetPreviewLoadFailed", "界面设置预览未能应用。")));
				BroadcastEditStateChanged();
				return false;
			}
			if (bEndEditRequested) return false;
		}
	}
	OnSettingValueChanged.Broadcast(InSettingId);
	if (bEndEditRequested) return false;
	BroadcastEditStateChanged();
	return true;
}

FSettingEditState USettingModule::GetEditState() const
{
	FSettingEditState State;
	State.bActive = EditSession.bActive;
	State.bCanApply = CanApply();
	State.bCanReset = CanReset();
	State.bDirty = State.bCanApply;
	return State;
}

void USettingModule::BroadcastEditStateChanged()
{
	OnSettingEditStateChanged.Broadcast(GetEditState());
}

void USettingModule::RegisterProvider(USettingProviderBase* InProvider)
{
	if (!IsBusinessAdmissionOpen()) return;
	if (!InProvider)
	{
		return;
	}
	Providers.AddUnique(InProvider);
	if (!FinalDefinitions.IsEmpty())
	{
		BuildSettingDefinitions();
	}
}

void USettingModule::UnregisterProvider(USettingProviderBase* InProvider)
{
	if (Providers.Remove(InProvider) > 0 && IsBusinessAdmissionOpen() && !FinalDefinitions.IsEmpty())
	{
		BuildSettingDefinitions();
	}
}

TArray<USettingEntry*> USettingModule::GetSettingEntries() const
{
	TArray<USettingEntry*> Result;
	Result.Reserve(SettingEntries.Num());
	for (USettingEntry* Entry : SettingEntries)
	{
		Result.Add(Entry);
	}
	return Result;
}

TArray<USettingEntry*> USettingModule::GetSettingEntriesByPage(FName InPage) const
{
	TArray<USettingEntry*> Result;
	for (USettingEntry* Entry : SettingEntries)
	{
		if (Entry && Entry->GetDefinition().Page == InPage)
		{
			Result.Add(Entry);
		}
	}
	return Result;
}

TArray<FSettingPageDefinition> USettingModule::GetSettingPages() const
{
	TArray<FSettingPageDefinition> Result = SettingConfig.Pages;
	for (const FSettingDefinition& Definition : FinalDefinitions)
	{
		if (Definition.Page.IsNone() || Result.ContainsByPredicate(
		                                    [&Definition](const FSettingPageDefinition& Page)
		                                    {
			                                    return Page.Page == Definition.Page;
		                                    }))
		{
			continue;
		}

		FSettingPageDefinition& Page = Result.AddDefaulted_GetRef();
		Page.Page = Definition.Page;
		Page.DisplayName = FText::FromName(Definition.Page);
		Page.Order = GetDefaultSettingPageOrder(Definition.Page);
	}
	Result.Sort(
	    [](const FSettingPageDefinition& A, const FSettingPageDefinition& B)
	    {
		    return A.Order == B.Order ? A.Page.LexicalLess(B.Page) : A.Order < B.Order;
	    });
	return Result;
}

void USettingModule::LoadData(const FParameter& InSaveData, EPhase InPhase)
{
	if (!IsBusinessAdmissionOpen())
	{
		MarkLoadDataFailed();
		return;
	}
	UWidgetModule* Widget = UWidgetModule::GetPtr(GetWorld());
	UAudioModule* Audio = UAudioModule::GetPtr(GetWorld());
	UVideoModule* Video = UVideoModule::GetPtr(GetWorld());
	UCameraModule* Camera = UCameraModule::GetPtr(GetWorld());
	UInputModule* Input = UInputModule::GetPtr(GetWorld());
	UParameterModule* Parameters = UParameterModule::GetPtr(GetWorld());
	if (!Widget || !Audio || !Video || !Camera || !Input || !Parameters)
	{
		UE_LOG(LogTemp, Error, TEXT("无法加载当前世界的完整设置：必要模块不可用。"));
		return;
	}
	auto& SaveData = InSaveData.GetRef<FSettingModuleSaveData>();
	if (!Widget->LoadSaveData(FParameter(SaveData.WidgetData), InPhase)) return;
	if (!Audio->LoadSaveData(FParameter(SaveData.AudioData), InPhase)) return;
	if (!Video->LoadSaveData(FParameter(SaveData.VideoData), InPhase)) return;
	if (!Camera->LoadSaveData(FParameter(SaveData.CameraData), InPhase)) return;
	if (!Input->LoadSaveData(FParameter(SaveData.InputData), InPhase)) return;
	if (!Parameters->LoadSaveData(FParameter(SaveData.ParameterData), InPhase)) return;
	MarkLocalProfilePhaseComplete();
}

void USettingModule::UnloadData(EPhase InPhase)
{
}

FParameter USettingModule::ToData()
{
	FSettingModuleSaveData SaveData;
	if (!GetCurrentCombinedSettings(SaveData)) return FParameter();
	return FParameter(MoveTemp(SaveData));
}

void USettingModule::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
}
