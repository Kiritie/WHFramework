// Fill out your copyright notice in the Description page of Project Settings.


#include "Asset/AssetModule.h"
#include "Asset/AssetModuleTypes.h"
		
IMPLEMENTATION_MODULE(UAssetModule)

// Sets default values
UAssetModule::UAssetModule()
{
	ModuleName = FName("AssetModule");
	ModuleDisplayName = FText::FromString(TEXT("Asset Module"));
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
}
#endif

void UAssetModule::OnInitialize()
{
	Super::OnInitialize();
}

void UAssetModule::OnPreparatory(EPhase InPhase)
{
	Super::OnPreparatory(InPhase);

	if(PHASEC(InPhase, EPhase::Primary))
	{
		for(auto& Iter : StaticClasses)
		{
			FStaticClass& StaticClass = Iter.Value;
			StaticClass.LoadedClass = StaticClass.IsNeedLoad() ? LoadClass(StaticClass.BaseClass, StaticClass.GetClassName()) : FindClass(StaticClass.GetClassName());
		}
		
		for(auto& Iter : StaticObjects)
		{
			FStaticObject& StaticObject = Iter.Value;
			StaticObject.LoadedObject = StaticObject.IsNeedLoad() ? LoadObject(StaticObject.BaseClass, StaticObject.GetObjectName()) : FindObject(StaticObject.BaseClass, StaticObject.GetObjectName());
		}
		
		for(auto Iter : DataAssets)
		{
			if(Iter)
			{
				DataAssetMap.Add(Iter->GetDataAssetName(), Iter);
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
	}
}

void UAssetModule::OnRefresh(float DeltaSeconds)
{
	Super::OnRefresh(DeltaSeconds);
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

bool UAssetModule::HasDataAsset(FName InName) const
{
	return DataAssetMap.Contains(InName);
}

UDataAssetBase* UAssetModule::GetDataAsset(TSubclassOf<UDataAssetBase> InClass, FName InName) const
{
	if(!InClass) return nullptr;
	
	if(InName.IsNone()) InName = InClass.GetDefaultObject()->GetDataAssetName();

	return GetDataAsset<UDataAssetBase>(InName);
}

UDataAssetBase* UAssetModule::CreateDataAsset(TSubclassOf<UDataAssetBase> InClass, FName InName)
{
	if(!InClass) return nullptr;

	if(InName.IsNone()) InName = InClass.GetDefaultObject()->GetDataAssetName();

	return CreateDataAsset<UDataAssetBase>(InName);
}

bool UAssetModule::RemoveDataAsset(TSubclassOf<UDataAssetBase> InClass, FName InName)
{
	if(!InClass) return false;

	if(InName.IsNone()) InName = InClass.GetDefaultObject()->GetDataAssetName();

	return RemoveDataAsset<UDataAssetBase>(InName);
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
