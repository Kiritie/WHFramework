#include "Voxel/VoxelDetailBaker.h"
#include "Voxel/Rendering/VoxelDetailData.h"
#include "Voxel/VoxelEditorAssetIO.h"
#include "Voxel/Serialization/VoxelBinaryCodec.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"
#include "UObject/Package.h"
namespace
{
bool IntPosition(FVoxelByteWriter& W,const FVector3f& P)
{
    for(int32 A=0;A<3;++A)
    {
        const double Q=double(P[A])*100.0;
        if(!FMath::IsFinite(Q)||FMath::Abs(Q)>2000000000.0)return false;
        W.I32(FMath::RoundToInt(Q));
    }
    return true;
}
}
bool FVoxelDetailBaker::Bake(UVoxelDetailData& A,const FString& Root,FString& E)
{
    check(IsInGameThread());
    if(A.StableKey.IsNone()||!Root.StartsWith(TEXT("/Game/"))){E=TEXT("Detail key / output root invalid");return false;}
    UStaticMesh* Mesh=nullptr;
    const bool Native=!A.SourceVerticesCm.IsEmpty()||!A.SourceTriangles.IsEmpty();
    if(Native)
    {
        if(A.SourceVerticesCm.Num()>65536||A.SourceTriangles.IsEmpty()||A.SourceTriangles.Num()>196608||A.SourceTriangles.Num()%3)
        {E=TEXT("Invalid native detail triangle count");return false;}
        auto* Material=A.SourceMaterial.LoadSynchronous();
        if(!Material){E=TEXT("Native detail requires a material asset");return false;}
        FMeshDescription Desc;FStaticMeshAttributes Attr(Desc);Attr.Register();
        auto Positions=Attr.GetVertexPositions();auto Normals=Attr.GetVertexInstanceNormals();
        auto Tangents=Attr.GetVertexInstanceTangents();auto Signs=Attr.GetVertexInstanceBinormalSigns();
        auto Colors=Attr.GetVertexInstanceColors();auto UV=Attr.GetVertexInstanceUVs();UV.SetNumChannels(1);
        const FPolygonGroupID Group=Desc.CreatePolygonGroup();
        Attr.GetPolygonGroupMaterialSlotNames()[Group]=TEXT("Detail");
        TArray<FVertexID> V;
        for(const FVector& P:A.SourceVerticesCm)
        {
            if(P.ContainsNaN()||P.GetAbsMax()>100000.0){E=TEXT("Native detail vertex out of bounds");return false;}
            auto ID=Desc.CreateVertex();Positions[ID]=FVector3f(P);V.Add(ID);
        }
        for(int32 I=0;I<A.SourceTriangles.Num();I+=3)
        {
            int32 J[3]={A.SourceTriangles[I],A.SourceTriangles[I+1],A.SourceTriangles[I+2]};
            for(int32 K:J)if(!V.IsValidIndex(K)){E=TEXT("Detail index out of range");return false;}
            FVector P0=A.SourceVerticesCm[J[0]],P1=A.SourceVerticesCm[J[1]],P2=A.SourceVerticesCm[J[2]];
            FVector N=FVector::CrossProduct(P1-P0,P2-P0).GetSafeNormal();
            if(N.IsNearlyZero()){E=TEXT("Degenerate detail triangle");return false;}
            FVector T=(P1-P0).GetSafeNormal();TArray<FVertexInstanceID> Corners;
            for(int32 K=0;K<3;++K)
            {
                auto ID=Desc.CreateVertexInstance(V[J[K]]);Corners.Add(ID);
                Normals[ID]=FVector3f(N);Tangents[ID]=FVector3f(T);Signs[ID]=1.f;Colors[ID]=FVector4f(1,1,1,1);
                const FVector& P=A.SourceVerticesCm[J[K]];
                UV.Set(ID,0,FVector2f(float(P.X/100.0),float(P.Y/100.0)));
            }
            Desc.CreatePolygon(Group,Corners);
        }
        // Generated asset only; source meshes and source prefabs are never overwritten.
        const FString Path=Root/TEXT("SM_")+A.GetName();
        Mesh=Cast<UStaticMesh>(FVoxelEditorAssetIO::LoadOrCreate(UStaticMesh::StaticClass(),Path,E));
        if(!Mesh)return false;
        Mesh->Modify();Mesh->SetNumSourceModels(1);
        Mesh->GetStaticMaterials().Reset();Mesh->GetStaticMaterials().Add(FStaticMaterial(Material,TEXT("Detail")));
        Mesh->CreateMeshDescription(0,MoveTemp(Desc));Mesh->CommitMeshDescription(0);
        TArray<FText> BuildErrors;Mesh->Build(true,&BuildErrors);
        if(!BuildErrors.IsEmpty()){E=BuildErrors[0].ToString();return false;}
        if(!FVoxelEditorAssetIO::Save(Mesh,E))return false;
    }
    else Mesh=A.Mesh.LoadSynchronous();
    if(!Mesh){E=TEXT("Missing detail mesh");return false;}
    FMeshDescription Desc;
    if(!Mesh->CloneMeshDescription(0,Desc)||Desc.Triangles().Num()>65536)
    {E=TEXT("Detail needs a bounded LOD0 mesh description for deterministic baking");return false;}
    FStaticMeshAttributes Attr(Desc);auto P=Attr.GetVertexPositions();
    FVoxelByteWriter W(32*1024*1024);W.U32(1);W.String(A.StableKey.ToString());
    W.U32(Desc.Triangles().Num());
    for(const FTriangleID T:Desc.Triangles().GetElementIDs())
        for(const FVertexInstanceID V:Desc.GetTriangleVertexInstances(T))
            if(!IntPosition(W,P[Desc.GetVertexInstanceVertex(V)])){E=TEXT("Detail position cannot be canonicalized");return false;}
    for(const auto& M:Mesh->GetStaticMaterials())W.String(M.MaterialInterface?M.MaterialInterface->GetPathName():FString());
    W.U32(A.CollisionBoxesCm.Num());
    if(A.CollisionBoxesCm.Num()>32){E=TEXT("A detail allows at most 32 simple boxes");return false;}
    for(const FBox& B:A.CollisionBoxesCm)
    {
        if(!B.IsValid||B.Min.ContainsNaN()||B.Max.ContainsNaN()||B.GetSize().GetMin()<=0||
           !IntPosition(W,FVector3f(B.Min))||!IntPosition(W,FVector3f(B.Max)))
        {E=TEXT("Invalid detail collision bounds");return false;}
    }
    for(int32 Axis=0;Axis<3;++Axis){W.I32(A.FootprintCells.Min[Axis]);W.I32(A.FootprintCells.Max[Axis]);}
    W.U8(A.bNearCollision);TArray<uint8> Bytes;
    if(!W.Finish(Bytes)){E=TEXT("Detail hash exceeds budget");return false;}
    A.Modify();A.Mesh=Mesh;A.GeometryHash=VoxelBinary::Hash(Bytes);A.BakeVersion=1;A.MetadataHash=A.ComputeMetadataHash();
    if(!A.Validate(E))return false;
    return FVoxelEditorAssetIO::Save(&A,E);
}
