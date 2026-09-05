#include "Dialogue/Commandlet/DialogueSnapshotCommandlet.h"

#include "Dialogue/Base/DialogueAsset.h"
#include "Dialogue/Base/DialogueConditionBase.h"
#include "Dialogue/Base/DialogueEventBase.h"
#include "Sound/DialogueWave.h"
#include "Sound/SoundBase.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Internationalization/Text.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectHash.h"

UDialogueSnapshotCommandlet::UDialogueSnapshotCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
}

namespace DialogueSnapshot
{
	TSharedRef<FJsonObject> Text(const FText& Value)
	{
		TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
		Result->SetStringField(TEXT("display"), Value.ToString());
		FString Exported;
		FTextStringHelper::WriteToBuffer(Exported, Value);
		Result->SetStringField(TEXT("serialized"), Exported);
		return Result;
	}

	TSharedRef<FJsonObject> Object(UObject* Value)
	{
		TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
		Result->SetStringField(TEXT("path"), Value->GetPathName());
		Result->SetStringField(TEXT("outer"), Value->GetOuter() ? Value->GetOuter()->GetPathName() : FString());
		Result->SetStringField(TEXT("class"), Value->GetClass()->GetPathName());
		TSharedRef<FJsonObject> Properties = MakeShared<FJsonObject>();
		for (TFieldIterator<FProperty> It(Value->GetClass()); It; ++It)
		{
			FProperty* Property = *It;
			if (Property->HasAnyPropertyFlags(CPF_Transient | CPF_DuplicateTransient | CPF_NonPIEDuplicateTransient)) continue;
			FString Exported;
			Property->ExportTextItem_Direct(Exported, Property->ContainerPtrToValuePtr<void>(Value), nullptr, Value, PPF_None);
			Properties->SetStringField(Property->GetName(), Exported);
		}
		Result->SetObjectField(TEXT("properties"), Properties);
		return Result;
	}

	template<class T>
	TArray<TSharedPtr<FJsonValue>> ObjectPaths(const TArray<T*>& Values)
	{
		TArray<TSharedPtr<FJsonValue>> Result;
		for (const T* Value : Values)
		{
			if (Value) Result.Add(MakeShared<FJsonValueString>(Value->GetPathName()));
			else Result.Add(MakeShared<FJsonValueNull>());
		}
		return Result;
	}
}

int32 UDialogueSnapshotCommandlet::Main(const FString& Params)
{
	FString Output;
	if (!FParse::Value(*Params, TEXT("Output="), Output))
	{
		UE_LOG(LogTemp, Error, TEXT("Specify -Output=<snapshot.json>. This command only reads assets."));
		return 1;
	}
	if (IFileManager::Get().FileExists(*Output))
	{
		UE_LOG(LogTemp, Error, TEXT("Snapshot already exists: %s"), *Output);
		return 1;
	}
	IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	Registry.SearchAllAssets(true);
	const FTopLevelAssetPath OldClass(TEXT("/Script/DialoguePlugin"), TEXT("Dialogue"));
	TSet<FTopLevelAssetPath> Classes;
	Registry.GetDerivedClassNames({UDialogueAsset::StaticClass()->GetClassPathName(), OldClass}, {}, Classes);
	Classes.Add(UDialogueAsset::StaticClass()->GetClassPathName());
	Classes.Add(OldClass);
	TArray<FAssetData> Assets;
	Registry.GetAllAssets(Assets, true);
	Assets.Sort([](const FAssetData& A, const FAssetData& B) { return A.GetSoftObjectPath().ToString() < B.GetSoftObjectPath().ToString(); });
	TArray<TSharedPtr<FJsonValue>> Dialogues;
	int32 TotalNodes = 0;
	int32 FailedAssets = 0;
	for (const FAssetData& Asset : Assets)
	{
		if (!Classes.Contains(Asset.AssetClassPath)) continue;
		UDialogueAsset* Dialogue = Cast<UDialogueAsset>(Asset.GetAsset());
		if (!Dialogue)
		{
			UE_LOG(LogTemp, Error, TEXT("Cannot read dialogue: %s"), *Asset.GetSoftObjectPath().ToString());
			++FailedAssets;
			continue;
		}
		TSharedRef<FJsonObject> Entry = DialogueSnapshot::Object(Dialogue);
		Entry->SetStringField(TEXT("asset_registry_class"), Asset.AssetClassPath.ToString());
		Entry->SetObjectField(TEXT("name"), DialogueSnapshot::Text(Dialogue->Name));
		Entry->SetNumberField(TEXT("next_node_id"), Dialogue->NextNodeId);
		TArray<TSharedPtr<FJsonValue>> Nodes;
		for (const FDialogueNode& Node : Dialogue->Data)
		{
			TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
			Item->SetNumberField(TEXT("id"), Node.id);
			Item->SetBoolField(TEXT("is_player"), Node.isPlayer);
			Item->SetObjectField(TEXT("text"), DialogueSnapshot::Text(Node.Text));
			Item->SetNumberField(TEXT("x"), Node.Coordinates.X);
			Item->SetNumberField(TEXT("y"), Node.Coordinates.Y);
			TArray<TSharedPtr<FJsonValue>> Links;
			for (int32 Link : Node.Links) Links.Add(MakeShared<FJsonValueNumber>(Link));
			Item->SetArrayField(TEXT("links"), Links);
			Item->SetArrayField(TEXT("conditions"), DialogueSnapshot::ObjectPaths(Node.Conditions));
			Item->SetArrayField(TEXT("events"), DialogueSnapshot::ObjectPaths(Node.Events));
			Item->SetStringField(TEXT("sound"), GetPathNameSafe(Node.Sound));
			Item->SetStringField(TEXT("dialogue_wave"), GetPathNameSafe(Node.DialogueWave));
			Nodes.Add(MakeShared<FJsonValueObject>(Item));
		}
		TotalNodes += Nodes.Num();
		Entry->SetArrayField(TEXT("nodes"), Nodes);
		TArray<UObject*> Subobjects;
		GetObjectsWithOuter(Dialogue, Subobjects, true);
		Subobjects.Sort([](const UObject& A, const UObject& B) { return A.GetPathName() < B.GetPathName(); });
		TArray<TSharedPtr<FJsonValue>> Objects;
		for (UObject* Subobject : Subobjects) Objects.Add(MakeShared<FJsonValueObject>(DialogueSnapshot::Object(Subobject)));
		Entry->SetArrayField(TEXT("subobjects"), Objects);
		TArray<FName> References;
		Registry.GetReferencers(Asset.PackageName, References);
		References.Sort(FNameLexicalLess());
		TArray<TSharedPtr<FJsonValue>> Referencers;
		for (FName Reference : References) Referencers.Add(MakeShared<FJsonValueString>(Reference.ToString()));
		Entry->SetArrayField(TEXT("referencers"), Referencers);
		Dialogues.Add(MakeShared<FJsonValueObject>(Entry));
	}
	TSharedRef<FJsonObject> Document = MakeShared<FJsonObject>();
	Document->SetNumberField(TEXT("format_version"), 1);
	Document->SetStringField(TEXT("created_utc"), FDateTime::UtcNow().ToIso8601());
	Document->SetNumberField(TEXT("dialogue_count"), Dialogues.Num());
	Document->SetNumberField(TEXT("node_count"), TotalNodes);
	Document->SetNumberField(TEXT("failed_assets"), FailedAssets);
	Document->SetArrayField(TEXT("dialogues"), Dialogues);
	FString Json;
	FJsonSerializer::Serialize(Document, TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&Json));
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Output), true);
	if (!FFileHelper::SaveStringToFile(Json, *Output, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)) return 1;
	UE_LOG(LogTemp, Display, TEXT("Recorded %d dialogues, %d nodes, %d failed assets: %s"), Dialogues.Num(), TotalNodes, FailedAssets, *Output);
	return FailedAssets == 0 && !Dialogues.IsEmpty() ? 0 : 1;
}
