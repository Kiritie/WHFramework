#include "Voxel/VoxelPrefabImportCommandlet.h"
#include "Voxel/VoxelAssetMigrationTool.h"
#include "Voxel/VoxelEditorAssetIO.h"
#include "Voxel/Prefabs/Data/VoxelPrefabData.h"
#include "Voxel/Runtime/VoxelRegistry.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "HAL/FileManager.h"
#include "UObject/StrongObjectPtr.h"
#include <cmath>

namespace
{
    bool OnlyKeys(const FJsonObject& O, const TSet<FString>& Allowed)
    {
        for (const auto& P : O.Values) if (!Allowed.Contains(FString(P.Key.ToView()))) return false;
        return true;
    }
    bool ReadInteger(const TSharedPtr<FJsonValue>& V, int32 Min, int32 Max, int32& Out)
    {
        double N = 0;
        if (!V || !V->TryGetNumber(N) || !FMath::IsFinite(N) ||
            N < Min || N > Max || std::floor(N) != N) return false;
        Out = int32(N);
        return true;
    }
    bool ReadAssetId(const FString& Text, FPrimaryAssetId& Out)
    {
        FString Type, Name;
        if (!Text.Split(TEXT(":"), &Type, &Name) || Type.IsEmpty() ||
            Name.IsEmpty() || Name.Contains(TEXT(":"))) return false;
        Out = FPrimaryAssetId(FName(*Type), FName(*Name));
        return Out.IsValid();
    }
    struct FStagedPrefab
    {
        TStrongObjectPtr<UVoxelPrefabData> Asset;
        FVoxelPrefabSaveData Data;
        FText DisplayName;
        bool bSetDisplayName = false;
    };
}

UVoxelPrefabImportCommandlet::UVoxelPrefabImportCommandlet()
{
    IsClient = false; IsServer = false; IsEditor = true;
    LogToConsole = true; ShowErrorCount = true;
}

int32 UVoxelPrefabImportCommandlet::Main(const FString& Params)
{
    FString Plan, Text, Error;
    if (!FParse::Value(*Params, TEXT("Plan="), Plan) || Plan.IsEmpty())
    {
        UE_LOG(LogTemp, Error, TEXT("Plan= is required"));
        return 1;
    }
    const int64 Bytes = IFileManager::Get().FileSize(*Plan);
    if (Bytes < 0 || Bytes > 32ll * 1024 * 1024 || !FFileHelper::LoadFileToString(Text, *Plan))
    {
        UE_LOG(LogTemp, Error, TEXT("Prefab plan is missing or exceeds 32 MiB"));
        return 2;
    }
    TSharedPtr<FJsonObject> Root;
    const TArray<TSharedPtr<FJsonValue>>* Rows = nullptr;
    double Version = 0;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root ||
        !OnlyKeys(*Root, {TEXT("Version"), TEXT("Rows")}) ||
        !Root->TryGetNumberField(TEXT("Version"), Version) || Version != 1 ||
        !Root->TryGetArrayField(TEXT("Rows"), Rows) || Rows->IsEmpty() || Rows->Num() > 4096)
    {
        UE_LOG(LogTemp, Error, TEXT("Invalid prefab plan root; expected Version=1 and Rows"));
        return 3;
    }
    TArray<UVoxelData*> VoxelAssets;
    FVoxelRegistry Registry;
    if (!FVoxelAssetMigrationTool::Scan(VoxelAssets, Error) || !Registry.Build(VoxelAssets, false, Error))
    {
        UE_LOG(LogTemp, Error, TEXT("Voxel registry is not ready: %s"), *Error);
        return 4;
    }
    TArray<FStagedPrefab> Staged;
    TSet<FString> SeenAssets;
    for (int32 RowIndex = 0; RowIndex < Rows->Num(); ++RowIndex)
    {
        const auto& JsonRow = (*Rows)[RowIndex];
        const auto Row = JsonRow && JsonRow->Type == EJson::Object ? JsonRow->AsObject() : nullptr;
        FString AssetPath, DisplayName;
        const TArray<TSharedPtr<FJsonValue>>* Cells = nullptr;
        if (!Row || !OnlyKeys(*Row, {TEXT("Asset"), TEXT("DisplayName"), TEXT("Cells")}) ||
            !Row->TryGetStringField(TEXT("Asset"), AssetPath) ||
            !Row->TryGetArrayField(TEXT("Cells"), Cells) || Cells->Num() > 4096 ||
            SeenAssets.Contains(AssetPath))
        {
            UE_LOG(LogTemp, Error, TEXT("Invalid or duplicate prefab row %d"), RowIndex);
            return 5;
        }
        FStagedPrefab Entry;
        Entry.Asset.Reset(LoadObject<UVoxelPrefabData>(nullptr, *AssetPath));
        if (!Entry.Asset.IsValid())
        {
            UE_LOG(LogTemp, Error, TEXT("Existing prefab asset could not be loaded: %s"), *AssetPath);
            return 6;
        }
        const FString Canonical = Entry.Asset->GetPathName();
        if (SeenAssets.Contains(Canonical))
        {
            UE_LOG(LogTemp, Error, TEXT("Two rows resolve to the same prefab asset: %s"), *Canonical);
            return 7;
        }
        SeenAssets.Add(Canonical); SeenAssets.Add(AssetPath);
        if (Row->HasField(TEXT("DisplayName")))
        {
            if (!Row->TryGetStringField(TEXT("DisplayName"), DisplayName)) return 8;
            Entry.bSetDisplayName = true;
            Entry.DisplayName = FText::FromString(DisplayName);
        }
        for (int32 CellIndex = 0; CellIndex < Cells->Num(); ++CellIndex)
        {
            const auto& JsonCell = (*Cells)[CellIndex];
            const auto Cell = JsonCell && JsonCell->Type == EJson::Object ? JsonCell->AsObject() : nullptr;
            const TArray<TSharedPtr<FJsonValue>>* Offset = nullptr;
            FString AssetId;
            if (!Cell || !OnlyKeys(*Cell, {TEXT("Offset"), TEXT("AssetId"), TEXT("State")}) ||
                !Cell->TryGetArrayField(TEXT("Offset"), Offset) || Offset->Num() != 3 ||
                !Cell->TryGetStringField(TEXT("AssetId"), AssetId) || !Cell->HasField(TEXT("State")))
            {
                UE_LOG(LogTemp, Error, TEXT("Invalid prefab cell %d:%d"), RowIndex, CellIndex);
                return 9;
            }
            FVoxelPrefabCell Out;
            for (int32 Axis = 0; Axis < 3; ++Axis)
            {
                if (!ReadInteger((*Offset)[Axis], -VoxelBlock::MaxAbsCoordinate + 1,
                    VoxelBlock::MaxAbsCoordinate - 1, Out.Offset[Axis]))
                {
                    UE_LOG(LogTemp, Error, TEXT("Invalid integer offset %d:%d"), RowIndex, CellIndex);
                    return 10;
                }
            }
            if (!ReadInteger(Cell->TryGetField(TEXT("State")), 0, 65535, Out.Item.State) ||
                !ReadAssetId(AssetId, Out.Item.VoxelAssetID))
            {
                UE_LOG(LogTemp, Error, TEXT("Invalid State or AssetId %d:%d"), RowIndex, CellIndex);
                return 11;
            }
            Out.Item.Count = 1;
            Entry.Data.Cells.Add(MoveTemp(Out));
        }
        if (!UVoxelPrefabData::ValidateCells(Entry.Data, *Registry.GetSnapshot(), Error))
        {
            UE_LOG(LogTemp, Error, TEXT("Prefab %s: %s"), *AssetPath, *Error);
            return 12;
        }
        Staged.Add(MoveTemp(Entry));
    }
    if (FParse::Param(*Params, TEXT("CheckOnly")))
    {
        UE_LOG(LogTemp, Display, TEXT("Validated %d prefab rows; no assets modified"), Staged.Num());
        return 0;
    }
    int32 Saved = 0;
    for (FStagedPrefab& Entry : Staged)
    {
        Entry.Asset->Modify();
        Entry.Asset->Data = MoveTemp(Entry.Data);
        if (Entry.bSetDisplayName) Entry.Asset->DisplayName = Entry.DisplayName;
        if (!FVoxelEditorAssetIO::Save(Entry.Asset.Get(), Error))
        {
            UE_LOG(LogTemp, Error, TEXT("Prefab save failed after %d successful saves: %s. Keep the export and inspect source-control changes."), Saved, *Error);
            return 13;
        }
        ++Saved;
    }
    UE_LOG(LogTemp, Display, TEXT("Saved %d prefab assets"), Saved);
    return 0;
}
