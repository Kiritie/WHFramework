#include "Voxel/VoxelLegacyExportCommandlet.h"
#include "Voxel/Voxels/Data/VoxelData.h"
#include "Voxel/Prefabs/Data/VoxelPrefabData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "JsonObjectConverter.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Math/MathTypes.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
UVoxelLegacyExportCommandlet::UVoxelLegacyExportCommandlet()
{
	IsClient = false;
	IsServer = false;
	IsEditor = true;
	LogToConsole = true;
	ShowErrorCount = true;
}
int32 UVoxelLegacyExportCommandlet::Main(const FString& Params)
{
	FString Out;
	if (!FParse::Value(*Params, TEXT("Out="), Out) || Out.IsEmpty())
	{
		UE_LOG(LogTemp, Error, TEXT("Out= is required"));
		return 1;
	}
	auto& AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
	AR.SearchAllAssets(true);
	TArray<FAssetData> Assets;
	AR.GetAssetsByClass(UVoxelData::StaticClass()->GetClassPathName(), Assets, true);
	Assets.Sort(
	    [](const auto& A, const auto& B)
	    {
		    return A.GetSoftObjectPath().ToString() < B.GetSoftObjectPath().ToString();
	    });
	TArray<TSharedPtr<FJsonValue>> Rows;
	for (const auto& Entry : Assets)
	{
		auto* A = Cast<UVoxelData>(Entry.GetAsset());
		if (!A)
			return 2;
		auto Row = MakeShared<FJsonObject>(), Properties = MakeShared<FJsonObject>();
		if (!FJsonObjectConverter::UStructToJsonObject(A->GetClass(), A, Properties, 0, CPF_Transient))
			return 3;
		Row->SetStringField(TEXT("Asset"), A->GetPathName());
		Row->SetStringField(TEXT("Class"), A->GetClass()->GetPathName());
		Row->SetStringField(TEXT("PrimaryAssetId"), A->GetPrimaryAssetId().ToString());
		Row->SetObjectField(TEXT("Properties"), Properties);
		Rows.Add(MakeShared<FJsonValueObject>(Row));
	}
	TArray<FAssetData> Prefabs;
	AR.GetAssetsByClass(UVoxelPrefabData::StaticClass()->GetClassPathName(), Prefabs, true);
	Prefabs.Sort(
	    [](const FAssetData& A, const FAssetData& B)
	    {
		    return A.GetSoftObjectPath().ToString() < B.GetSoftObjectPath().ToString();
	    });
	TArray<TSharedPtr<FJsonValue>> PrefabRows;
	for (const FAssetData& Entry : Prefabs)
	{
		auto* A = Cast<UVoxelPrefabData>(Entry.GetAsset());
		if (!A)
			return 6;
		auto Row = MakeShared<FJsonObject>(), Properties = MakeShared<FJsonObject>();
		if (!FJsonObjectConverter::UStructToJsonObject(A->GetClass(), A, Properties, 0, CPF_Transient))
			return 7;
		Row->SetStringField(TEXT("Asset"), A->GetPathName());
		Row->SetStringField(TEXT("Class"), A->GetClass()->GetPathName());
		Row->SetStringField(TEXT("PrimaryAssetId"), A->GetPrimaryAssetId().ToString());
		Row->SetObjectField(TEXT("Properties"), Properties);
		PrefabRows.Add(MakeShared<FJsonValueObject>(Row));
	}
	auto Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("Version"), 1);
	Root->SetArrayField(TEXT("Rows"), Rows);
	Root->SetArrayField(TEXT("PrefabRows"), PrefabRows);
	Root->SetStringField(TEXT("CoordinatePolicy"),
	                     TEXT("Keep original prefab strings verbatim. Decode with the recorded original world settings; never infer packed coordinates."));
	// Explicit old UV index order -> new +X,-X,+Y,-Y,+Z,-Z. No hardcoded enum ordinals.
	TArray<TSharedPtr<FJsonValue>> Faces;
	for (auto F : {EDirectionN::Forward, EDirectionN::Backward, EDirectionN::Right, EDirectionN::Left, EDirectionN::Up, EDirectionN::Down})
		Faces.Add(MakeShared<FJsonValueNumber>(int32(F)));
	Root->SetArrayField(TEXT("FaceOrder"), Faces);
	FString Text;
	if (!FJsonSerializer::Serialize(Root, TJsonWriterFactory<>::Create(&Text)))
		return 4;
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Out), true);
	if (!FFileHelper::SaveStringToFile(Text, *Out, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		return 5;
	UE_LOG(LogTemp, Display, TEXT("Exported %d voxel assets; source assets were not modified"), Rows.Num());
	return 0;
}
