#include "Voxel/VoxelValidateCommandlet.h"
#include "Voxel/Voxels/Data/VoxelData.h"
#include "Voxel/Prefabs/Data/VoxelPrefabData.h"
#include "Voxel/Rendering/VoxelMaterialSet.h"
#include "Voxel/Runtime/VoxelRegistry.h"
#include "Voxel/Generation/Assets/VoxelWorldGenerationProfile.h"
#include "Voxel/Generation/VoxelGenerationBinding.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/Texture2D.h"
#include "Engine/Texture2DArray.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceConstant.h"
#include "MaterialEditingLibrary.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/PackageName.h"
#include "HAL/FileManager.h"
#include "UObject/StrongObjectPtr.h"

UVoxelValidateCommandlet::UVoxelValidateCommandlet()
{
    IsClient = false; IsServer = false; IsEditor = true;
    LogToConsole = true; ShowErrorCount = true;
}

int32 UVoxelValidateCommandlet::Main(const FString& Params)
{
    FString SetPath = TEXT("/Game/VoxelGenerated/DA_VoxelMaterials.DA_VoxelMaterials");
    FString ReportPath = FPaths::ProjectSavedDir() / TEXT("Voxel/asset_validation.json");
    FParse::Value(*Params, TEXT("Set="), SetPath);
    FParse::Value(*Params, TEXT("Report="), ReportPath);
    if (!SetPath.Contains(TEXT(".")))
        SetPath += TEXT(".") + FPackageName::GetLongPackageAssetName(SetPath);
    TArray<TSharedPtr<FJsonValue>> Issues;
    auto Issue = [&Issues](const FString& Asset, const TCHAR* Code, const FString& Message)
    {
        auto Row = MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("Asset"), Asset);
        Row->SetStringField(TEXT("Code"), Code);
        Row->SetStringField(TEXT("Message"), Message);
        Issues.Add(MakeShared<FJsonValueObject>(Row));
        UE_LOG(LogTemp, Error, TEXT("%s [%s] %s"), *Asset, Code, *Message);
    };
    TStrongObjectPtr<UVoxelMaterialSet> Set(LoadObject<UVoxelMaterialSet>(nullptr, *SetPath));
    FString Error;
    if (!Set.IsValid() || !Set->Validate(Error))
        Issue(SetPath, TEXT("MaterialSet"), Error.IsEmpty() ? TEXT("MaterialSet could not be loaded") : Error);
    if (Set.IsValid())
    {
        for (const FVoxelMaterialBank& Bank : Set->Banks)
        {
            UTexture2DArray* Texture = Bank.Textures;
            UMaterialInstanceConstant* MIC = Cast<UMaterialInstanceConstant>(Bank.Material.Get());
            if (!Texture || !Texture->Source.IsValid() || Texture->Source.GetSizeX() <= 0 ||
                Texture->Source.GetSizeX() != Texture->Source.GetSizeY() || !Texture->SRGB)
                Issue(SetPath, TEXT("TextureArray"), FString::Printf(TEXT("Invalid source data in group %d bank %d"), int32(Bank.Group), Bank.Bank));
            if (!MIC || UMaterialEditingLibrary::GetMaterialInstanceTextureParameterValue(MIC, TEXT("VoxelArray"),
                    EMaterialParameterAssociation::GlobalParameter) != Texture)
                Issue(SetPath, TEXT("MaterialBinding"), FString::Printf(TEXT("VoxelArray binding differs in group %d bank %d"), int32(Bank.Group), Bank.Bank));
        }
    }
    auto& AR = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
    AR.SearchAllAssets(true);
    TArray<FAssetData> Found;
    AR.GetAssetsByClass(UVoxelData::StaticClass()->GetClassPathName(), Found, true);
    Found.Sort([](const FAssetData& A, const FAssetData& B)
    { return A.GetSoftObjectPath().ToString() < B.GetSoftObjectPath().ToString(); });
    TArray<TStrongObjectPtr<UVoxelData>> Pinned;
    TArray<UVoxelData*> Assets;
    TSet<FName> Names;
    TSet<FPrimaryAssetId> IDs;
    for (const FAssetData& Entry : Found)
    {
        TStrongObjectPtr<UVoxelData> Asset(Cast<UVoxelData>(Entry.GetAsset()));
        if (!Asset.IsValid())
        {
            Issue(Entry.GetSoftObjectPath().ToString(), TEXT("AssetLoad"), TEXT("Voxel asset failed to load"));
            continue;
        }
        if (!Asset->bRegisterBlock) continue;
        UVoxelData* A = Asset.Get();
        const FString Path = A->GetPathName();
        Error.Reset();
        if (!A->ValidateDefinition(true, Error)) Issue(Path, TEXT("Definition"), Error);
        if (!A->GetPrimaryAssetId().IsValid() || IDs.Contains(A->GetPrimaryAssetId()))
            Issue(Path, TEXT("PrimaryAssetId"), TEXT("Invalid or duplicate item/voxel identity"));
        if (Names.Contains(A->BlockName)) Issue(Path, TEXT("BlockName"), TEXT("Duplicate stable name"));
        Names.Add(A->BlockName); IDs.Add(A->GetPrimaryAssetId());
        for (int32 Half = 0; Half < (A->Shape == EVoxelShapeKind::Door ? 2 : 1); ++Half)
        {
            const auto& Source = Half ? A->UpperFaceMaterials : A->FaceMaterials;
            const auto& Baked = Half ? A->BakedUpperFaces : A->BakedFaces;
            for (uint8 Face = 0; Face < 6; ++Face)
            {
                const FVoxelFaceTexture& FaceTexture = Source.Get(Face);
                UTexture2D* Texture = FaceTexture.Texture.LoadSynchronous();
                if (!Texture || !Texture->Source.IsValid() || FaceTexture.FrameCount < 1 ||
                    FaceTexture.FrameCount > 256 || FaceTexture.FramesPerSecond < 0 ||
                    FaceTexture.FramesPerSecond > 60 || Texture->Source.GetSizeX() <= 0 ||
                    Texture->Source.GetSizeX() > 4096 ||
                    int64(Texture->Source.GetSizeY()) != int64(Texture->Source.GetSizeX()) * FaceTexture.FrameCount)
                {
                    Issue(Path, TEXT("SourceFrames"), FString::Printf(TEXT("Invalid texture/frame strip at half %d face %d"), Half, Face));
                }
                FVoxelRuntimeFaceRef Ref;
                if (!Baked.IsValidIndex(Face) || !Baked[Face].ToRuntime(Ref))
                {
                    Issue(Path, TEXT("BakedFace"), FString::Printf(TEXT("Invalid baked half %d face %d"), Half, Face));
                    continue;
                }
                const FVoxelMaterialBank* Bank = Set.IsValid() ? Set->FindBank(A->RenderGroup, Ref.Bank) : nullptr;
                if (!Bank || uint32(Ref.Layer) + Ref.Frames > uint32(Bank->SliceCount) ||
                    Ref.Frames != FaceTexture.FrameCount || Ref.FPS != FaceTexture.FramesPerSecond)
                {
                    Issue(Path, TEXT("LayerRange"), FString::Printf(TEXT("Bank/layer/animation mismatch at half %d face %d"), Half, Face));
                }
            }
        }
        Assets.Add(A);
        Pinned.Add(MoveTemp(Asset));
    }
    FVoxelRegistry Registry;
    Error.Reset();
    const bool bRegistryOK = Registry.Build(Assets, true, Error);
    if (!bRegistryOK) Issue(TEXT("Registry"), TEXT("Build"), Error);

    TArray<FAssetData> Profiles;
    AR.GetAssetsByClass(UVoxelWorldGenerationProfile::StaticClass()->GetClassPathName(), Profiles, true);
    Profiles.Sort([](const FAssetData& A, const FAssetData& B)
    { return A.GetSoftObjectPath().ToString() < B.GetSoftObjectPath().ToString(); });
    for (const FAssetData& Entry : Profiles)
    {
        TStrongObjectPtr<UVoxelWorldGenerationProfile> Profile(Cast<UVoxelWorldGenerationProfile>(Entry.GetAsset()));
        Error.Reset();
        if (!Profile.IsValid())
        {
            Issue(Entry.GetSoftObjectPath().ToString(), TEXT("GenerationProfileLoad"), TEXT("Generation profile failed to load"));
        }
        else if (!bRegistryOK)
        {
            Issue(Profile->GetPathName(), TEXT("GenerationProfileBlocked"), TEXT("Cannot validate generation profile without a valid registry"));
        }
        else
        {
            FVoxelGenerationRuntimeConfig Config;
            if (!FVoxelGenerationBinding::Build(*Profile, *Registry.GetSnapshot(), Profile->Defaults, Profile->TargetCellCentimeters, Config, Error))
                Issue(Profile->GetPathName(), TEXT("GenerationProfile"), Error);
        }
    }

    TArray<FAssetData> Prefabs;
    AR.GetAssetsByClass(UVoxelPrefabData::StaticClass()->GetClassPathName(), Prefabs, true);
    Prefabs.Sort([](const FAssetData& A, const FAssetData& B)
    { return A.GetSoftObjectPath().ToString() < B.GetSoftObjectPath().ToString(); });
    for (const FAssetData& Entry : Prefabs)
    {
        TStrongObjectPtr<UVoxelPrefabData> Asset(Cast<UVoxelPrefabData>(Entry.GetAsset()));
        Error.Reset();
        if (!Asset.IsValid()) Issue(Entry.GetSoftObjectPath().ToString(), TEXT("PrefabLoad"), TEXT("Prefab failed to load"));
        else if (!bRegistryOK) Issue(Asset->GetPathName(), TEXT("PrefabBlocked"), TEXT("Cannot validate prefab references without a valid registry"));
        else if (!Asset->Validate(*Registry.GetSnapshot(), Error)) Issue(Asset->GetPathName(), TEXT("Prefab"), Error);
    }
    auto Report = MakeShared<FJsonObject>();
    Report->SetStringField(TEXT("Scope"), TEXT("Voxel/Prefab definitions and directly referenced rendering assets only; no Blueprint graph inspection"));
    Report->SetNumberField(TEXT("VoxelAssets"), Assets.Num());
    Report->SetNumberField(TEXT("GenerationProfiles"), Profiles.Num());
    Report->SetNumberField(TEXT("PrefabAssets"), Prefabs.Num());
    Report->SetBoolField(TEXT("Passed"), Issues.IsEmpty());
    Report->SetArrayField(TEXT("Issues"), Issues);
    FString Text;
    if (!FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Text)) ||
        !IFileManager::Get().MakeDirectory(*FPaths::GetPath(ReportPath), true) ||
        !FFileHelper::SaveStringToFile(Text, *ReportPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
    {
        UE_LOG(LogTemp, Error, TEXT("Asset report could not be written: %s"), *ReportPath);
        return 2;
    }
    UE_LOG(LogTemp, Display, TEXT("Validated %d voxel, %d generation profile and %d prefab assets; %d issues. Report: %s"),
        Assets.Num(), Profiles.Num(), Prefabs.Num(), Issues.Num(), *ReportPath);
    return Issues.IsEmpty() ? 0 : 1;
}
