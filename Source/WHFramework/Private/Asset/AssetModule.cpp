// Fill out your copyright notice in the Description page of Project Settings.


#include "Asset/AssetModule.h"
#include "Asset/AssetModuleTypes.h"
		
IMPLEMENTATION_MODULE(AAssetModule)

// Sets default values
AAssetModule::AAssetModule()
{
	ModuleName = FName("AssetModule");
}

AAssetModule::~AAssetModule()
{
	TERMINATION_MODULE(AAssetModule)
}

#if WITH_EDITOR
void AAssetModule::OnGenerate()
{
	Super::OnGenerate();
}

void AAssetModule::OnDestroy()
{
	Super::OnDestroy();
}
#endif

void AAssetModule::OnInitialize_Implementation()
{
	Super::OnInitialize_Implementation();
}

void AAssetModule::OnPreparatory_Implementation(EPhase InPhase)
{
	Super::OnPreparatory_Implementation(InPhase);

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

void AAssetModule::OnRefresh_Implementation(float DeltaSeconds)
{
	Super::OnRefresh_Implementation(DeltaSeconds);
}

void AAssetModule::OnPause_Implementation()
{
	Super::OnPause_Implementation();
}

void AAssetModule::OnUnPause_Implementation()
{
	Super::OnUnPause_Implementation();
}

void AAssetModule::OnTermination_Implementation(EPhase InPhase)
{
	Super::OnTermination_Implementation(InPhase);

	if(PHASEC(InPhase, EPhase::Final))
	{
		UAssetManagerBase::Get().ReleaseRuntimeData();
	}
}

void AAssetModule::AddStaticClass(const FName InName, const FStaticClass& InStaticClass)
{
	StaticClasses.Add(InName, InStaticClass);
}

UClass* AAssetModule::GetStaticClass(const FName InName)
{
	return StaticClasses[InName].LoadedClass;
}

UClass* AAssetModule::FindClass(const FString& InName, bool bExactClass)
{
	if(!ClassMappings.Contains(InName))
	{
		ClassMappings.Add(InName, Cast<UClass>(StaticFindObject(UClass::StaticClass(), nullptr, *InName, bExactClass)));
	}
	return ClassMappings[InName];
}

UClass* AAssetModule::LoadClass(UClass* InClass, const FString& InName)
{
	if(!ClassMappings.Contains(InName))
	{
		ClassMappings.Add(InName, StaticLoadClass(InClass, nullptr, *InName));
	}
	return ClassMappings[InName];
}

void AAssetModule::AddStaticObject(const FName InName, const FStaticObject& InStaticObject)
{
	StaticObjects.Add(InName, InStaticObject);
}

UObject* AAssetModule::GetStaticObject(UClass* InClass, const FName InName)
{
	return StaticObjects[InName].LoadedObject;
}

UObject* AAssetModule::FindObject(UClass* InClass, const FString& InName, bool bExactClass)
{
	if(!ObjectMappings.Contains(InName))
	{
		ObjectMappings.Add(InName, StaticFindObject(InClass, nullptr, *InName, bExactClass));
	}
	return ObjectMappings[InName];
}

UObject* AAssetModule::LoadObject(UClass* InClass, const FString& InName)
{
	if(!ObjectMappings.Contains(InName))
	{
		ObjectMappings.Add(InName, StaticLoadObject(InClass, nullptr, *InName));
	}
	return ObjectMappings[InName];
}

UEnum* AAssetModule::FindEnumByValue(const FString& InEnumName, int32 InEnumValue, bool bExactClass)
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

UEnum* AAssetModule::FindEnumByAuthoredName(const FString& InEnumName, const FString& InEnumAuthoredName, bool bExactClass)
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

UEnum* AAssetModule::FindEnumByDisplayName(const FString& InEnumName, const FString& InEnumDisplayName, bool bExactClass)
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

void AAssetModule::AddEnumMapping(const FString& InEnumName, const FString& InOtherName)
{
	FEnumMapping& Mappings = EnumMappings.FindOrAdd(InEnumName);
	if(!Mappings.EnumNames.Contains(InOtherName))
	{
		Mappings.EnumNames.Add(InOtherName);
	}
}

void AAssetModule::RemoveEnumMapping(const FString& InEnumName, const FString& InOtherName)
{
	FEnumMapping& Mappings = EnumMappings.FindOrAdd(InEnumName);
	if(Mappings.EnumNames.Contains(InOtherName))
	{
		Mappings.EnumNames.Remove(InOtherName);
	}
}

bool AAssetModule::HasDataAsset(FName InName) const
{
	return DataAssetMap.Contains(InName);
}

UDataAssetBase* AAssetModule::GetDataAsset(TSubclassOf<UDataAssetBase> InClass, FName InName) const
{
	if(!InClass) return nullptr;
	
	if(InName.IsNone()) InName = InClass.GetDefaultObject()->GetDataAssetName();

	return GetDataAsset<UDataAssetBase>(InName);
}

UDataAssetBase* AAssetModule::CreateDataAsset(TSubclassOf<UDataAssetBase> InClass, FName InName)
{
	if(!InClass) return nullptr;

	if(InName.IsNone()) InName = InClass.GetDefaultObject()->GetDataAssetName();

	return CreateDataAsset<UDataAssetBase>(InName);
}

bool AAssetModule::RemoveDataAsset(TSubclassOf<UDataAssetBase> InClass, FName InName)
{
	if(!InClass) return false;

	if(InName.IsNone()) InName = InClass.GetDefaultObject()->GetDataAssetName();

	return RemoveDataAsset<UDataAssetBase>(InName);
}

void AAssetModule::RemoveAllDataAsset()
{
	for (auto Iter : DataAssetMap)
	{
		if(!Iter.Value) continue;
		Iter.Value->ConditionalBeginDestroy();
	}
	DataAssetMap.Empty();
}

bool AAssetModule::AddDataTable(UDataTable* InDataTable)
{
	if(!InDataTable) return false;
		
	if(!DataTableMap.Contains(InDataTable->RowStruct))
	{
		DataTableMap.Add(InDataTable->RowStruct, InDataTable);
		return true;
	}
	return false;
}

bool AAssetModule::RemoveDataTable(UDataTable* InDataTable)
{
	if(!InDataTable) return false;
		
	if(DataTableMap.Contains(InDataTable->RowStruct))
	{
		DataTableMap.Remove(InDataTable->RowStruct);
		return true;
	}
	return false;
}

void AAssetModule::RemoveAllDataTable()
{
	DataTableMap.Empty();
}
