// Fill out your copyright notice in the Description page of Project Settings.


#include "Asset/AssetModule.h"

#include "Asset/AssetManagerBase.h"
#include "Asset/AssetModuleTypes.h"
		
IMPLEMENTATION_MODULE(UAssetModule)

// Sets default values
UAssetModule::UAssetModule()
{
	ModuleName = FName("AssetModule");
	ModuleDisplayName = FText::FromString(TEXT("Asset Module"));

	bModuleRequired = true;
}

UAssetModule::~UAssetModule()
{
	TERMINATION_MODULE(UAssetModule)
}

#if WITH_EDITOR
void UAssetModule::OnGenerate()
{
	Super::OnGenerate();
}

void UAssetModule::OnDestroy()
{
	Super::OnDestroy();

	TERMINATION_MODULE(UAssetModule)
}
#endif

void UAssetModule::OnInitialize()
{
	Super::OnInitialize();
		
	for(auto Iter : DataAssets)
	{
		if(Iter)
		{
			DataAssetMap.Add(Iter->GetDataAssetTag(), Iter);
		}
	}
		
	for(auto Iter : DataTables)
	{
		if(Iter)
		{
			FString ContextStr;
			Iter->ForeachRow<FDataTableRowBase>(ContextStr, [](const FName& Key, const FDataTableRowBase& Value)
			{
				const_cast<FDataTableRowBase&>(Value).OnInitializeRow(Key);
			});
			DataTableMap.Add(Iter->RowStruct, Iter);
		}
	}

	for(auto& Iter : StaticClasses)
	{
		FStaticClass& StaticClass = Iter.Value;
		StaticClass.LoadedClass = LoadClass(StaticClass.BaseClass, StaticClass.GetClassName());
	}
		
	for(auto& Iter : StaticObjects)
	{
		FStaticObject& StaticObject = Iter.Value;
		StaticObject.LoadedObject = LoadObject(StaticObject.BaseClass, StaticObject.GetObjectName());
	}
}

void UAssetModule::OnPreparatory(EPhase InPhase)
{
	Super::OnPreparatory(InPhase);
}

void UAssetModule::OnRefresh(float DeltaSeconds, bool bInEditor)
{
	Super::OnRefresh(DeltaSeconds, bInEditor);
}

void UAssetModule::OnPause()
{
	Super::OnPause();
}

void UAssetModule::OnUnPause()
{
	Super::OnUnPause();
}

void UAssetModule::OnTermination(EPhase InPhase)
{
	Super::OnTermination(InPhase);

	if(PHASEC(InPhase, EPhase::Final))
	{
		UAssetManagerBase::Get().ReleaseRuntimeData();
	}
}

void UAssetModule::AddStaticClass(const FName InName, const FStaticClass& InStaticClass)
{
	StaticClasses.Add(InName, InStaticClass);
}

UClass* UAssetModule::GetStaticClass(const FName InName)
{
	return StaticClasses[InName].LoadedClass;
}

UClass* UAssetModule::FindClass(const FString& InName, bool bExactClass)
{
	if(!ClassMappings.Contains(InName))
	{
		ClassMappings.Add(InName, Cast<UClass>(StaticFindObject(UClass::StaticClass(), nullptr, *InName, bExactClass)));
	}
	return ClassMappings[InName];
}

UClass* UAssetModule::LoadClass(UClass* InClass, const FString& InName)
{
	if(!ClassMappings.Contains(InName))
	{
		ClassMappings.Add(InName, StaticLoadClass(InClass, nullptr, *InName));
	}
	return ClassMappings[InName];
}

void UAssetModule::AddStaticObject(const FName InName, const FStaticObject& InStaticObject)
{
	StaticObjects.Add(InName, InStaticObject);
}

UObject* UAssetModule::GetStaticObject(UClass* InClass, const FName InName)
{
	return StaticObjects[InName].LoadedObject;
}

UObject* UAssetModule::FindObject(UClass* InClass, const FString& InName, bool bExactClass)
{
	if(!ObjectMappings.Contains(InName))
	{
		ObjectMappings.Add(InName, StaticFindObject(InClass, nullptr, *InName, bExactClass));
	}
	return ObjectMappings[InName];
}

UObject* UAssetModule::LoadObject(UClass* InClass, const FString& InName)
{
	if(!ObjectMappings.Contains(InName))
	{
		ObjectMappings.Add(InName, StaticLoadObject(InClass, nullptr, *InName));
	}
	return ObjectMappings[InName];
}

UEnum* UAssetModule::FindEnumByValue(const FString& InEnumName, int32 InEnumValue, bool bExactClass)
{
	if(EnumMappings.Contains(InEnumName))
	{
		for(auto& Iter : EnumMappings[InEnumName].EnumNames)
		{
			if(UEnum* Enum = FindObject<UEnum>(Iter, bExactClass))
			{
				if(Enum->GetMaxEnumValue() != InEnumValue && Enum->IsValidEnumValue(InEnumValue))
				{
					return Enum;
				}
			}
		}
	}
	return FindObject<UEnum>(InEnumName, bExactClass);
}

UEnum* UAssetModule::FindEnumByAuthoredName(const FString& InEnumName, const FString& InEnumAuthoredName, bool bExactClass)
{
	if(EnumMappings.Contains(InEnumName))
	{
		for(auto& Iter : EnumMappings[InEnumName].EnumNames)
		{
			if(UEnum* Enum = FindObject<UEnum>(Iter, bExactClass))
			{
				if(Enum->IsValidEnumName(*InEnumAuthoredName))
				{
					return Enum;
				}
			}
		}
	}
	return FindObject<UEnum>(InEnumName, bExactClass);
}

UEnum* UAssetModule::FindEnumByDisplayName(const FString& InEnumName, const FString& InEnumDisplayName, bool bExactClass)
{
	if(EnumMappings.Contains(InEnumName))
	{
		for(auto& Iter : EnumMappings[InEnumName].EnumNames)
		{
			if(UEnum* Enum = FindObject<UEnum>(Iter, bExactClass))
			{
				for(int32 i = 0; i < Enum->NumEnums() - 1; i++)
				{
					if(Enum->GetDisplayNameTextByIndex(i).ToString().Equals(InEnumDisplayName))
					{
						return Enum;
					}
				}
			}
		}
	}
	return FindObject<UEnum>(InEnumName, bExactClass);
}

void UAssetModule::AddEnumMapping(const FString& InEnumName, const FString& InOtherName)
{
	FEnumMapping& Mappings = EnumMappings.FindOrAdd(InEnumName);
	if(!Mappings.EnumNames.Contains(InOtherName))
	{
		Mappings.EnumNames.Add(InOtherName);
	}
}

void UAssetModule::RemoveEnumMapping(const FString& InEnumName, const FString& InOtherName)
{
	FEnumMapping& Mappings = EnumMappings.FindOrAdd(InEnumName);
	if(Mappings.EnumNames.Contains(InOtherName))
	{
		Mappings.EnumNames.Remove(InOtherName);
	}
}

bool UAssetModule::HasDataAsset(const FGameplayTag& InTag) const
{
	return DataAssetMap.Contains(InTag);
}

UDataAssetBase* UAssetModule::GetDataAsset(TSubclassOf<UDataAssetBase> InClass, const FGameplayTag& InTag) const
{
	if(!InClass) return nullptr;
	
	const FGameplayTag& Tag = InTag.IsValid() ? InTag : InClass->GetDefaultObject<UDataAssetBase>()->GetDataAssetTag();

	return GetDataAsset<UDataAssetBase>(Tag);
}

UDataAssetBase* UAssetModule::CreateDataAsset(TSubclassOf<UDataAssetBase> InClass, const FGameplayTag& InTag)
{
	if(!InClass) return nullptr;

	const FGameplayTag& Tag = InTag.IsValid() ? InTag : InClass->GetDefaultObject<UDataAssetBase>()->GetDataAssetTag();

	return CreateDataAsset<UDataAssetBase>(Tag);
}

bool UAssetModule::RemoveDataAsset(TSubclassOf<UDataAssetBase> InClass, const FGameplayTag& InTag)
{
	if(!InClass) return false;

	const FGameplayTag& Tag = InTag.IsValid() ? InTag : InClass->GetDefaultObject<UDataAssetBase>()->GetDataAssetTag();

	return RemoveDataAsset<UDataAssetBase>(Tag);
}

void UAssetModule::RemoveAllDataAsset()
{
	for (auto Iter : DataAssetMap)
	{
		if(!Iter.Value) continue;
		Iter.Value->ConditionalBeginDestroy();
	}
	DataAssetMap.Empty();
}

bool UAssetModule::AddDataTable(UDataTable* InDataTable)
{
	if(!InDataTable) return false;
		
	if(!DataTableMap.Contains(InDataTable->RowStruct))
	{
		DataTableMap.Add(InDataTable->RowStruct, InDataTable);
		return true;
	}
	return false;
}

bool UAssetModule::RemoveDataTable(UDataTable* InDataTable)
{
	if(!InDataTable) return false;
		
	if(DataTableMap.Contains(InDataTable->RowStruct))
	{
		DataTableMap.Remove(InDataTable->RowStruct);
		return true;
	}
	return false;
}

void UAssetModule::RemoveAllDataTable()
{
	DataTableMap.Empty();
}
