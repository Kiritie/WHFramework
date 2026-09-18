#include "Voxel/VoxelAssetMigrationTool.h"
#include "Voxel/VoxelEditorAssetIO.h"
#include "Voxel/Voxels/Data/VoxelData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "JsonObjectConverter.h"
#include "Misc/FileHelper.h"
#include "UObject/StrongObjectPtr.h"
bool FVoxelAssetMigrationTool::Scan(TArray<UVoxelData*>& Out,FString& Error)
{
    Out.Reset();auto& AR=FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry")).Get();
    AR.SearchAllAssets(true);TArray<FAssetData> Found;AR.GetAssetsByClass(UVoxelData::StaticClass()->GetClassPathName(),Found,true);
    Found.Sort([](const FAssetData&A,const FAssetData&B){return A.GetSoftObjectPath().ToString()<B.GetSoftObjectPath().ToString();});
    TSet<FName> Names;
    for(const auto& F:Found)
    {
        auto* A=Cast<UVoxelData>(F.GetAsset());
        if(A&&!A->bRegisterBlock)continue;
        if(!A||!A->ValidateDefinition(false,Error)||Names.Contains(A->BlockName))
        {Error=F.GetSoftObjectPath().ToString()+TEXT(": ")+Error;return false;}
        Names.Add(A->BlockName);Out.Add(A);
    }
    if(Out.IsEmpty()){Error=TEXT("No voxel assets found");return false;}return true;
}
bool FVoxelAssetMigrationTool::ApplyPlan(const FString& Path,bool Write,TArray<UVoxelData*>& Out,FString& Error)
{
    Out.Reset();FString Text;TSharedPtr<FJsonObject> Root;
    if(!FFileHelper::LoadFileToString(Text,*Path)||Text.Len()>32*1024*1024||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Root)||!Root.IsValid())
    {Error=TEXT("Cannot parse migration plan");return false;}
    const TArray<TSharedPtr<FJsonValue>>* Rows=nullptr;
    if(!Root->TryGetArrayField(TEXT("Rows"),Rows)||Rows->IsEmpty()||Rows->Num()>65535){Error=TEXT("Invalid migration Rows");return false;}
    const TSet<FString> Allowed={TEXT("bRegisterBlock"),TEXT("blockName"),TEXT("shape"),TEXT("renderGroup"),TEXT("bSolid"),TEXT("bOccludes"),TEXT("bReplaceable"),TEXT("bBreakable"),TEXT("breakMilliseconds"),TEXT("dropAssetID"),TEXT("dropCount"),TEXT("entityKind"),TEXT("entityVariant"),TEXT("faceMaterials"),TEXT("upperFaceMaterials"),TEXT("sounds")};
    TArray<TStrongObjectPtr<UVoxelData>> Staged;TArray<UVoxelData*> Original;TSet<FString> Paths;TSet<FName> Names;
    for(const auto& Row:*Rows)
    {
        const auto Obj=Row->AsObject();FString AssetPath;const TSharedPtr<FJsonObject>* Def=nullptr;
        if(!Obj.IsValid()||!Obj->TryGetStringField(TEXT("Asset"),AssetPath)||!Obj->TryGetObjectField(TEXT("Definition"),Def)||Paths.Contains(AssetPath))
        {Error=TEXT("Invalid or duplicate migration row");return false;}
        for(const auto& Pair:(*Def)->Values){const FString Key(Pair.Key.ToView());if(!Allowed.Contains(Key)){Error=TEXT("Unapproved migration field: ")+Key;return false;}}
        UVoxelData* A=LoadObject<UVoxelData>(nullptr,*AssetPath);if(!A){Error=TEXT("Missing voxel asset: ")+AssetPath;return false;}
        TStrongObjectPtr<UVoxelData> Copy(DuplicateObject<UVoxelData>(A,GetTransientPackage()));
        if(!FJsonObjectConverter::JsonObjectToUStruct((*Def).ToSharedRef(),Copy->GetClass(),Copy.Get(),0,0)||!Copy->ValidateDefinition(false,Error)||(Copy->bRegisterBlock&&Names.Contains(Copy->BlockName)))
        {Error=AssetPath+TEXT(": invalid migrated definition: ")+Error;return false;}
        Copy->BakeVersion=0;Copy->BakedFaces.Reset();Copy->BakedUpperFaces.Reset();
        Paths.Add(AssetPath);if(Copy->bRegisterBlock)Names.Add(Copy->BlockName);Original.Add(A);Staged.Add(MoveTemp(Copy));
    }
    if(!Write){Out=Original;return true;}
    // Apply only the explicitly allowed voxel fields. Do not replace the shared Item asset or Item metadata.
    for(int32 I=0;I<Original.Num();++I)
    {
        UVoxelData& A=*Original[I];const UVoxelData& B=*Staged[I];A.Modify();
        A.bRegisterBlock=B.bRegisterBlock;A.BlockName=B.BlockName;A.Shape=B.Shape;A.RenderGroup=B.RenderGroup;A.bSolid=B.bSolid;A.bOccludes=B.bOccludes;
        A.bReplaceable=B.bReplaceable;A.bBreakable=B.bBreakable;A.BreakMilliseconds=B.BreakMilliseconds;
        A.DropAssetID=B.DropAssetID;A.DropCount=B.DropCount;A.EntityKind=B.EntityKind;A.EntityVariant=B.EntityVariant;
        A.FaceMaterials=B.FaceMaterials;A.UpperFaceMaterials=B.UpperFaceMaterials;A.Sounds=B.Sounds;
        A.BakeVersion=0;A.BakedFaces.Reset();A.BakedUpperFaces.Reset();
        if(!FVoxelEditorAssetIO::Save(&A,Error))return false;
    }
    Out=Original;return true;
}
