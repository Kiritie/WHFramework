#include "Parameter/Migration/ParameterMigrationCommandlet.h"

#include "Parameter/ParameterTypes.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Dom/JsonObject.h"
#include "Engine/DataTable.h"
#include "Internationalization/Text.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/SavePackage.h"
#include "UObject/UObjectHash.h"
#include "UObject/UnrealType.h"

UParameterMigrationCommandlet::UParameterMigrationCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
}

namespace ParameterMigration
{
	struct FContext
	{
		FString AssetPath;
		FString ObjectPath;
		FString OwnerType;
		TArray<TSharedPtr<FJsonValue>>& Records;
		TArray<TSharedPtr<FJsonValue>>& Errors;
		TSet<FString>& RecordKeys;
		const TMap<FString, TSharedPtr<FJsonObject>>* Imports = nullptr;
		int32* RestoredCount = nullptr;
		TSet<UPackage*>* ModifiedPackages = nullptr;
	};

	FString ExportProperty(FProperty* Property, const void* Value, UObject* Owner)
	{
		FString Exported;
		Property->ExportTextItem_Direct(Exported, Value, nullptr, Owner, PPF_None);
		return Exported;
	}

	void AddError(const FContext& Context, const FString& PropertyPath, const FParameter& Parameter, const FString& Error)
	{
		TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetStringField(TEXT("AssetPath"), Context.AssetPath);
		Entry->SetStringField(TEXT("ObjectPath"), Context.ObjectPath);
		Entry->SetStringField(TEXT("PropertyPath"), PropertyPath);
		Entry->SetStringField(TEXT("OldType"), StaticEnum<EParameterType>()->GetNameStringByValue(static_cast<int64>(Parameter.GetParameterType())));
		Entry->SetStringField(TEXT("Error"), Error);
		Context.Errors.Add(MakeShared<FJsonValueObject>(Entry));
	}

	void RecordParameter(const FContext& Context, const FString& PropertyPath, const FParameter& Parameter, UObject* Owner)
	{
		const FString RecordKey = Context.AssetPath + TEXT("|") + Context.ObjectPath + TEXT("|") + PropertyPath;
		if (Context.RecordKeys.Contains(RecordKey)) return;
		Context.RecordKeys.Add(RecordKey);

		const EParameterType ParameterType = Parameter.GetParameterType();
		const UEnum* ParameterTypeEnum = StaticEnum<EParameterType>();
		const FString TypeName = ParameterTypeEnum->GetNameStringByValue(static_cast<int64>(ParameterType));
		static const TMap<EParameterType, FName> ValuePropertyNames = {
			{EParameterType::Integer, TEXT("IntegerValue")}, {EParameterType::Float, TEXT("FloatValue")},
			{EParameterType::Byte, TEXT("ByteValue")}, {EParameterType::Enum, TEXT("EnumValue")},
			{EParameterType::String, TEXT("StringValue")}, {EParameterType::Name, TEXT("NameValue")},
			{EParameterType::Text, TEXT("TextValue")}, {EParameterType::Boolean, TEXT("BooleanValue")},
			{EParameterType::Vector, TEXT("VectorValue")}, {EParameterType::Rotator, TEXT("RotatorValue")},
			{EParameterType::Transform, TEXT("TransformValue")}, {EParameterType::Color, TEXT("ColorValue")},
			{EParameterType::LinearColor, TEXT("LinearColorValue")}, {EParameterType::Key, TEXT("KeyValue")},
			{EParameterType::Tag, TEXT("TagValue")}, {EParameterType::Tags, TEXT("TagsValue")},
			{EParameterType::Brush, TEXT("BrushValue")}, {EParameterType::Guid, TEXT("GuidValue")},
			{EParameterType::AssetID, TEXT("AssetIDValue")}, {EParameterType::Class, TEXT("ClassValue")},
			{EParameterType::ClassPtr, TEXT("ClassPtrValue")}, {EParameterType::Object, TEXT("ObjectValue")},
			{EParameterType::ObjectInst, TEXT("ObjectInstValue")}, {EParameterType::ObjectPtr, TEXT("ObjectPtrValue")},
			{EParameterType::Delegate, TEXT("DelegateValue")}
		};

		TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetStringField(TEXT("AssetPath"), Context.AssetPath);
		Entry->SetStringField(TEXT("ObjectPath"), Context.ObjectPath);
		Entry->SetStringField(TEXT("OwnerClassOrStruct"), Context.OwnerType);
		Entry->SetStringField(TEXT("PropertyPath"), PropertyPath);
		Entry->SetStringField(TEXT("OldParameterType"), TypeName);
		Entry->SetNumberField(TEXT("OldParameterTypeValue"), static_cast<uint8>(ParameterType));

		if (FProperty* DescriptionProperty = FindFProperty<FProperty>(FParameter::StaticStruct(), TEXT("Description")))
		{
			Entry->SetStringField(TEXT("Description"), ExportProperty(DescriptionProperty, DescriptionProperty->ContainerPtrToValuePtr<void>(&Parameter), Owner));
		}

		if (const FName* ValuePropertyName = ValuePropertyNames.Find(ParameterType))
		{
			FProperty* ValueProperty = FindFProperty<FProperty>(FParameter::StaticStruct(), *ValuePropertyName);
			const void* ValueAddress = ValueProperty ? ValueProperty->ContainerPtrToValuePtr<void>(&Parameter) : nullptr;
			if (!ValueProperty && Parameter.GetValueType() && Parameter.GetValueMemory())
			{
				ValueProperty = FindFProperty<FProperty>(Parameter.GetValueType(), TEXT("Value"));
				ValueAddress = ValueProperty ? ValueProperty->ContainerPtrToValuePtr<void>(Parameter.GetValueMemory()) : nullptr;
			}
			if (ValueProperty && ValueAddress)
			{
				Entry->SetStringField(TEXT("OldValueProperty"), ValuePropertyName->ToString());
				Entry->SetStringField(TEXT("OldValue"), ExportProperty(ValueProperty, ValueAddress, Owner));
				Entry->SetStringField(TEXT("OldValuePropertyType"), ValueProperty->GetClass()->GetName());
			}
		}
		else if (ParameterType == EParameterType::Pointer)
		{
			Entry->SetStringField(TEXT("Persistence"), TEXT("LocalOnly / UnsupportedForPersistentMigration"));
			AddError(Context, PropertyPath, Parameter, TEXT("Raw pointer values cannot be serialized or restored."));
		}
		else if (ParameterType != EParameterType::None && ParameterType != EParameterType::Misc)
		{
			AddError(Context, PropertyPath, Parameter, TEXT("No serializable value property mapping exists for this type."));
		}

		Context.Records.Add(MakeShared<FJsonValueObject>(Entry));
	}

	void RestoreParameter(FContext& Context, const FString& PropertyPath, FParameter& Parameter, UObject* Owner)
	{
		const FString RecordKey = Context.AssetPath + TEXT("|") + Context.ObjectPath + TEXT("|") + PropertyPath;
		const TSharedPtr<FJsonObject>* EntryPtr = Context.Imports ? Context.Imports->Find(RecordKey) : nullptr;
		if (!EntryPtr || !EntryPtr->IsValid())
		{
			AddError(Context, PropertyPath, Parameter, TEXT("No matching Before record."));
			return;
		}
		const TSharedPtr<FJsonObject>& Entry = *EntryPtr;
		const FString TypeName = Entry->GetStringField(TEXT("OldParameterType"));
		const int64 TypeValue = StaticEnum<EParameterType>()->GetValueByNameString(TypeName);
		if (TypeValue == INDEX_NONE)
		{
			AddError(Context, PropertyPath, Parameter, FString::Printf(TEXT("Unknown legacy type: %s"), *TypeName));
			return;
		}
		Parameter.SetParameterType(static_cast<EParameterType>(TypeValue));
		if (Parameter.HasValue() && Entry->HasField(TEXT("OldValue")))
		{
			FProperty* ValueProperty = FindFProperty<FProperty>(Parameter.GetValueType(), TEXT("Value"));
			void* ValueAddress = ValueProperty ? ValueProperty->ContainerPtrToValuePtr<void>(Parameter.GetMutableValueMemory()) : nullptr;
			const FString OldValue = Entry->GetStringField(TEXT("OldValue"));
			if (!ValueProperty || !ValueAddress || !ValueProperty->ImportText_Direct(*OldValue, ValueAddress, Owner, PPF_None))
			{
				AddError(Context, PropertyPath, Parameter, TEXT("Failed to import legacy value text."));
				return;
			}
		}
		if (FProperty* DescriptionProperty = FindFProperty<FProperty>(FParameter::StaticStruct(), TEXT("Description")))
		{
			const FString Description = Entry->GetStringField(TEXT("Description"));
			void* Address = DescriptionProperty->ContainerPtrToValuePtr<void>(&Parameter);
			if (!DescriptionProperty->ImportText_Direct(*Description, Address, Owner, PPF_None))
			{
				AddError(Context, PropertyPath, Parameter, TEXT("Failed to import Description."));
				return;
			}
		}
		Context.RecordKeys.Add(RecordKey);
		if (Context.RestoredCount) ++*Context.RestoredCount;
		if (Context.ModifiedPackages) Context.ModifiedPackages->Add(Owner->GetOutermost());
	}

	void ScanProperty(FProperty* Property, void* Value, const FString& Path, FContext& Context, UObject* Owner);

	void ScanStruct(const UStruct* Struct, void* Data, const FString& Path, FContext& Context, UObject* Owner)
	{
		for (TFieldIterator<FProperty> It(Struct); It; ++It)
		{
			FProperty* Property = *It;
			if (Property->HasAnyPropertyFlags(CPF_Transient | CPF_DuplicateTransient | CPF_NonPIEDuplicateTransient)) continue;
			const FString ChildPath = Path.IsEmpty() ? Property->GetName() : Path + TEXT(".") + Property->GetName();
			ScanProperty(Property, Property->ContainerPtrToValuePtr<void>(Data), ChildPath, Context, Owner);
		}
	}

	void ScanProperty(FProperty* Property, void* Value, const FString& Path, FContext& Context, UObject* Owner)
	{
		if (FStructProperty* StructProperty = CastField<FStructProperty>(Property))
		{
			if (StructProperty->Struct == FParameter::StaticStruct())
			{
				if (Context.Imports) RestoreParameter(Context, Path, *static_cast<FParameter*>(Value), Owner);
				else RecordParameter(Context, Path, *static_cast<FParameter*>(Value), Owner);
			}
			else
			{
				ScanStruct(StructProperty->Struct, Value, Path, Context, Owner);
			}
			return;
		}
		if (FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property))
		{
			FScriptArrayHelper Helper(ArrayProperty, Value);
			for (int32 Index = 0; Index < Helper.Num(); ++Index)
			{
				ScanProperty(ArrayProperty->Inner, Helper.GetRawPtr(Index), FString::Printf(TEXT("%s[%d]"), *Path, Index), Context, Owner);
			}
			return;
		}
		if (FMapProperty* MapProperty = CastField<FMapProperty>(Property))
		{
			FScriptMapHelper Helper(MapProperty, Value);
			for (int32 Index = 0; Index < Helper.GetMaxIndex(); ++Index)
			{
				if (!Helper.IsValidIndex(Index)) continue;
				const FString KeyText = ExportProperty(MapProperty->KeyProp, Helper.GetKeyPtr(Index), Owner);
				const FString ElementPath = FString::Printf(TEXT("%s{Key=%s}"), *Path, *KeyText.ReplaceCharWithEscapedChar());
				ScanProperty(MapProperty->KeyProp, Helper.GetKeyPtr(Index), ElementPath + TEXT(".Key"), Context, Owner);
				ScanProperty(MapProperty->ValueProp, Helper.GetValuePtr(Index), ElementPath + TEXT(".Value"), Context, Owner);
			}
			return;
		}
		if (FSetProperty* SetProperty = CastField<FSetProperty>(Property))
		{
			FScriptSetHelper Helper(SetProperty, Value);
			for (int32 Index = 0; Index < Helper.GetMaxIndex(); ++Index)
			{
				if (!Helper.IsValidIndex(Index)) continue;
				const FString ElementText = ExportProperty(SetProperty->ElementProp, Helper.GetElementPtr(Index), Owner);
				ScanProperty(SetProperty->ElementProp, Helper.GetElementPtr(Index), FString::Printf(TEXT("%s{Element=%s}"), *Path, *ElementText.ReplaceCharWithEscapedChar()), Context, Owner);
			}
		}
	}

	bool IsProjectAsset(const FAssetData& Asset)
	{
		const FString PackageName = Asset.PackageName.ToString();
		return PackageName.StartsWith(TEXT("/Game/")) || PackageName.StartsWith(TEXT("/WHFramework/"));
	}
}

int32 UParameterMigrationCommandlet::Main(const FString& Params)
{
	FString Mode = TEXT("Before");
	FParse::Value(*Params, TEXT("Mode="), Mode);
	const bool bImport = Mode.Equals(TEXT("Import"), ESearchCase::IgnoreCase);
	const bool bAfter = Mode.Equals(TEXT("After"), ESearchCase::IgnoreCase);
	FString Before = FPaths::ProjectSavedDir() / TEXT("FParameterMigration/FParameter_Before.json");
	FParse::Value(*Params, TEXT("Before="), Before);
	FString Output = FPaths::ProjectSavedDir() / (bAfter ? TEXT("FParameterMigration/FParameter_After.json") : TEXT("FParameterMigration/FParameter_Before.json"));
	FParse::Value(*Params, TEXT("Output="), Output);
	if (!bImport && IFileManager::Get().FileExists(*Output))
	{
		UE_LOG(LogTemp, Error, TEXT("Migration snapshot already exists: %s"), *Output);
		return 1;
	}

	TMap<FString, TSharedPtr<FJsonObject>> Imports;
	if (bImport || bAfter)
	{
		FString BeforeJson;
		TSharedPtr<FJsonObject> BeforeDocument;
		if (!FFileHelper::LoadFileToString(BeforeJson, *Before) ||
			!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(BeforeJson), BeforeDocument) || !BeforeDocument)
		{
			UE_LOG(LogTemp, Error, TEXT("Unable to load Before snapshot: %s"), *Before);
			return 1;
		}
		for (const TSharedPtr<FJsonValue>& Value : BeforeDocument->GetArrayField(TEXT("Parameters")))
		{
			const TSharedPtr<FJsonObject> Entry = Value->AsObject();
			const FString Key = Entry->GetStringField(TEXT("AssetPath")) + TEXT("|") + Entry->GetStringField(TEXT("ObjectPath")) + TEXT("|") + Entry->GetStringField(TEXT("PropertyPath"));
			Imports.Add(Key, Entry);
		}
	}

	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	Registry.SearchAllAssets(true);
	TArray<FAssetData> Assets;
	Registry.GetAllAssets(Assets, true);
	Assets.RemoveAll([](const FAssetData& Asset) { return !ParameterMigration::IsProjectAsset(Asset); });
	Assets.Sort([](const FAssetData& A, const FAssetData& B) { return A.PackageName.LexicalLess(B.PackageName); });

	TArray<TSharedPtr<FJsonValue>> Records;
	TArray<TSharedPtr<FJsonValue>> Errors;
	TSet<FString> RecordKeys;
	TSet<UPackage*> ModifiedPackages;
	int32 RestoredCount = 0;
	int32 FailedAssets = 0;
	for (const FAssetData& Asset : Assets)
	{
		UObject* AssetObject = Asset.GetAsset();
		if (!AssetObject)
		{
			++FailedAssets;
			continue;
		}
		UPackage* Package = AssetObject->GetOutermost();
		TArray<UObject*> Objects;
		GetObjectsWithPackage(Package, Objects, true);
		Objects.AddUnique(AssetObject);
		Objects.Sort([](const UObject& A, const UObject& B) { return A.GetPathName() < B.GetPathName(); });
		for (UObject* Object : Objects)
		{
			if (!IsValid(Object) || Object->HasAnyFlags(RF_Transient)) continue;
			ParameterMigration::FContext Context{Asset.PackageName.ToString(), Object->GetPathName(), Object->GetClass()->GetPathName(), Records, Errors, RecordKeys,
				bImport ? &Imports : nullptr, &RestoredCount, &ModifiedPackages};
			ParameterMigration::ScanStruct(Object->GetClass(), Object, FString(), Context, Object);
			if (UDataTable* DataTable = Cast<UDataTable>(Object))
			{
				if (const UScriptStruct* RowStruct = DataTable->GetRowStruct())
				{
					for (const TPair<FName, uint8*>& Row : DataTable->GetRowMap())
					{
						ParameterMigration::ScanStruct(RowStruct, Row.Value, FString::Printf(TEXT("Rows[%s]"), *Row.Key.ToString()), Context, DataTable);
					}
				}
			}
		}
	}

	if (bImport)
	{
		for (UPackage* Package : ModifiedPackages)
		{
			Package->MarkPackageDirty();
			const FString Filename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
			FSavePackageArgs SaveArgs;
			SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
			SaveArgs.SaveFlags = SAVE_NoError;
			if (!UPackage::SavePackage(Package, nullptr, *Filename, SaveArgs))
			{
				UE_LOG(LogTemp, Error, TEXT("Failed to save migrated package: %s"), *Package->GetName());
				++FailedAssets;
			}
		}
		for (const TPair<FString, TSharedPtr<FJsonObject>>& Import : Imports)
		{
			if (!RecordKeys.Contains(Import.Key))
			{
				TSharedRef<FJsonObject> Error = MakeShared<FJsonObject>();
				Error->SetStringField(TEXT("RecordKey"), Import.Key);
				Error->SetStringField(TEXT("Error"), TEXT("Before record was not found in loaded assets."));
				Errors.Add(MakeShared<FJsonValueObject>(Error));
			}
		}
		UE_LOG(LogTemp, Display, TEXT("Restored %d/%d FParameter values; packages=%d errors=%d"), RestoredCount, Imports.Num(), ModifiedPackages.Num(), Errors.Num() + FailedAssets);
		return RestoredCount == Imports.Num() && Errors.IsEmpty() && FailedAssets == 0 ? 0 : 1;
	}

	Records.Sort([](const TSharedPtr<FJsonValue>& A, const TSharedPtr<FJsonValue>& B)
	{
		const TSharedPtr<FJsonObject> AO = A->AsObject();
		const TSharedPtr<FJsonObject> BO = B->AsObject();
		return AO->GetStringField(TEXT("AssetPath")) + AO->GetStringField(TEXT("ObjectPath")) + AO->GetStringField(TEXT("PropertyPath")) <
			BO->GetStringField(TEXT("AssetPath")) + BO->GetStringField(TEXT("ObjectPath")) + BO->GetStringField(TEXT("PropertyPath"));
	});
	TSharedRef<FJsonObject> Document = MakeShared<FJsonObject>();
	Document->SetNumberField(TEXT("FormatVersion"), 1);
	Document->SetStringField(TEXT("CreatedUtc"), FDateTime::UtcNow().ToIso8601());
	Document->SetNumberField(TEXT("AssetCount"), Assets.Num());
	Document->SetNumberField(TEXT("FailedAssetCount"), FailedAssets);
	Document->SetNumberField(TEXT("ParameterCount"), Records.Num());
	Document->SetNumberField(TEXT("ErrorCount"), Errors.Num());
	Document->SetArrayField(TEXT("Parameters"), Records);
	Document->SetArrayField(TEXT("Errors"), Errors);
	if (bAfter)
	{
		for (const TSharedPtr<FJsonValue>& Value : Records)
		{
			const TSharedPtr<FJsonObject> Entry = Value->AsObject();
			const FString Key = Entry->GetStringField(TEXT("AssetPath")) + TEXT("|") + Entry->GetStringField(TEXT("ObjectPath")) + TEXT("|") + Entry->GetStringField(TEXT("PropertyPath"));
			const TSharedPtr<FJsonObject>* BeforeEntry = Imports.Find(Key);
			if (!BeforeEntry || (*BeforeEntry)->GetStringField(TEXT("OldParameterType")) != Entry->GetStringField(TEXT("OldParameterType")) ||
				(*BeforeEntry)->GetStringField(TEXT("OldValue")) != Entry->GetStringField(TEXT("OldValue")) ||
				(*BeforeEntry)->GetStringField(TEXT("Description")) != Entry->GetStringField(TEXT("Description")))
			{
				TSharedRef<FJsonObject> Error = MakeShared<FJsonObject>();
				Error->SetStringField(TEXT("RecordKey"), Key);
				Error->SetStringField(TEXT("Error"), TEXT("Before/After mismatch."));
				Errors.Add(MakeShared<FJsonValueObject>(Error));
			}
		}
		if (Records.Num() != Imports.Num())
		{
			TSharedRef<FJsonObject> Error = MakeShared<FJsonObject>();
			Error->SetStringField(TEXT("Error"), FString::Printf(TEXT("Parameter count mismatch: Before=%d After=%d"), Imports.Num(), Records.Num()));
			Errors.Add(MakeShared<FJsonValueObject>(Error));
		}
		Document->SetNumberField(TEXT("ErrorCount"), Errors.Num());
		Document->SetArrayField(TEXT("Errors"), Errors);
	}
	FString Json;
	FJsonSerializer::Serialize(Document, TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&Json));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Output), true);
	if (!FFileHelper::SaveStringToFile(Json, *Output, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)) return 1;
	UE_LOG(LogTemp, Display, TEXT("Recorded %d FParameter values from %d assets with %d errors: %s"), Records.Num(), Assets.Num(), Errors.Num() + FailedAssets, *Output);
	return Errors.IsEmpty() && FailedAssets == 0 ? 0 : 1;
}
