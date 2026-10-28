#include "Voxel/Runtime/VoxelRegistry.h"
#include "Voxel/Voxels/Data/VoxelData.h"
#include "Voxel/Geometry/VoxelShapeRegistry.h"
#include "Voxel/Serialization/VoxelBinaryCodec.h"
const FVoxelRuntimeDefinition* FVoxelRegistrySnapshot::Find(uint16 T)const
{return Definitions.IsValidIndex(T)?&Definitions[T]:nullptr;}
const FVoxelRuntimeDefinition* FVoxelRegistrySnapshot::Find(FName N)const
{const uint16*T=Names.Find(N);return T?Find(*T):nullptr;}
const FVoxelRuntimeDefinition* FVoxelRegistrySnapshot::Find(const FPrimaryAssetId&A)const
{const uint16*T=Assets.Find(A);return T?Find(*T):nullptr;}
bool FVoxelRegistrySnapshot::IsValid(FVoxelBlockState S)const
{
    if(S.TypeId==0)return S.State==0;const auto*D=Find(S.TypeId);
    return D&&FVoxelShapeRegistry::IsValidState(D->Shape,S.State)&&
        (D->StateGeometry.IsEmpty()||D->StateGeometry.Contains(uint16(S.State&~VoxelState::ObjectLockedMask)));
}
bool FVoxelRegistry::Build(const TArray<UVoxelData*>&In,bool Render,FString&E)
{
    check(IsInGameThread());if(In.Num()>65535){E=TEXT("Too many voxel types");return false;}
    TArray<UVoxelData*> Sorted;for(auto*A:In)if(A&&A->bRegisterBlock)Sorted.Add(A);
    for(UVoxelData*D:Sorted)if(!D||!D->ValidateDefinition(Render,E))return false;
    Sorted.Sort([](const UVoxelData&A,const UVoxelData&B){return A.BlockName.ToString().Compare(B.BlockName.ToString(),ESearchCase::CaseSensitive)<0;});
    auto R=MakeShared<FVoxelRegistrySnapshot,ESPMode::ThreadSafe>();
    FVoxelRuntimeDefinition Air;Air.BlockName=TEXT("core:air");Air.bReplaceable=true;Air.bBreakable=false;Air.DropCount=0;
    R->Definitions.Add(Air);R->Names.Add(Air.BlockName,0);
    FVoxelByteWriter W(32*1024*1024);W.U32(3);W.U32(uint32(Sorted.Num()));
    for(UVoxelData*A:Sorted)
    {
        if(R->Names.Contains(A->BlockName)||R->Assets.Contains(A->GetPrimaryAssetId())){E=TEXT("Duplicate name or asset");return false;}
        FVoxelRuntimeDefinition D;D.TypeId=uint16(R->Definitions.Num());D.BlockName=A->BlockName;D.AssetID=A->GetPrimaryAssetId();
        D.DropAssetID=A->DropAssetID.IsValid()?A->DropAssetID:D.AssetID;D.DropCount=A->DropCount;D.EntityKind=uint16(A->EntityKind);D.EntityVariant=uint8(A->EntityVariant);
        D.Shape=A->Shape;D.RenderGroup=A->RenderGroup;D.bSolid=A->bSolid;D.bOccludes=A->bOccludes;D.bReplaceable=A->bReplaceable;
        D.bBreakable=A->bBreakable;D.BreakMilliseconds=A->BreakMilliseconds;
        for (const FVoxelStateGeometry& Geometry : A->StateGeometry)
        {
            FVoxelResolvedShape& Resolved = D.StateGeometry.Add(uint16(Geometry.State));
            Resolved.CollisionBoxes = Geometry.CollisionBoxes;
            Resolved.SelectionBoxes = Geometry.SelectionBoxes;
            for (uint8 Face = 0; Face < 6; ++Face)
            {
                const int32 Axis = Face / 2;
                const int32 U = (Axis + 1) % 3;
                const int32 V = (Axis + 2) % 3;
                int32 Covered = 0;
                for (int32 Y = 0; Y < 16; ++Y) for (int32 X = 0; X < 16; ++X)
                {
                    // Cover the whole sample tile; thin openings must not become solid support.
                    const bool bCovered = Resolved.CollisionBoxes.ContainsByPredicate([&](const FBox& Box)
                    {
                        const bool bBoundary = (Face & 1) ? Box.Min[Axis] <= 0.0 : Box.Max[Axis] >= 1.0;
                        return bBoundary && Box.Min[U] <= double(X) / 16.0 && Box.Max[U] >= double(X + 1) / 16.0
                            && Box.Min[V] <= double(Y) / 16.0 && Box.Max[V] >= double(Y + 1) / 16.0;
                    });
                    if (!bCovered) continue;
                    const int32 Index = X + Y * 16;
                    Resolved.Coverage[Face][Index / 64] |= uint64(1) << (Index % 64);
                    ++Covered;
                }
                if (Covered == 256) Resolved.OcclusionMask |= uint8(1u << Face);
            }
        }
        if(Render)for(uint8 F=0;F<6;++F){if(!A->BakedFaces[F].ToRuntime(D.Faces[F]))return false;
            const FVoxelFaceTexture& Source=A->FaceMaterials.Get(F);
            D.Faces[F].Tint=Source.ShadingMode==EVoxelFaceShadingMode::PaletteColor?Source.PaletteColor:FLinearColor::White;
            D.UpperFaces[F]=D.Faces[F];
            if(D.Shape==EVoxelShapeKind::Door)
            {
                if(!A->BakedUpperFaces[F].ToRuntime(D.UpperFaces[F]))return false;
                const FVoxelFaceTexture& Upper=A->UpperFaceMaterials.Get(F);
                D.UpperFaces[F].Tint=Upper.ShadingMode==EVoxelFaceShadingMode::PaletteColor?Upper.PaletteColor:FLinearColor::White;
            }}
        R->Definitions.Add(D);R->Names.Add(D.BlockName,D.TypeId);R->Assets.Add(D.AssetID,D.TypeId);
        W.U16(D.TypeId);W.String(D.BlockName.ToString());W.String(D.AssetID.ToString());W.String(D.DropAssetID.ToString());
        W.U8(uint8(D.Shape));W.U8(uint8(D.RenderGroup));W.U8(D.bSolid);W.U8(D.bOccludes);W.U8(D.bReplaceable);W.U8(D.bBreakable);
        W.I32(D.BreakMilliseconds);W.I32(D.DropCount);W.U16(D.EntityKind);W.U8(D.EntityVariant);
        TArray<uint16> GeometryStates;
        D.StateGeometry.GetKeys(GeometryStates);
        GeometryStates.Sort();
        W.U32(uint32(GeometryStates.Num()));
        for (uint16 State : GeometryStates)
        {
            W.U16(State);
            const FVoxelResolvedShape& Geometry = D.StateGeometry.FindChecked(State);
            for (const TArray<FBox>* Boxes : {&Geometry.CollisionBoxes, &Geometry.SelectionBoxes})
            {
                W.U32(uint32(Boxes->Num()));
                for (const FBox& Box : *Boxes)
                {
                    for (int32 Axis = 0; Axis < 3; ++Axis)
                    {
                        W.I32(FMath::RoundToInt(Box.Min[Axis] * 1000000.));
                        W.I32(FMath::RoundToInt(Box.Max[Axis] * 1000000.));
                    }
                }
            }
        }
    }
    for(const auto&D:R->Definitions)if(D.DropCount>0&&!R->Assets.Contains(D.DropAssetID)){E=TEXT("Voxel drops require a registered voxel asset");return false;}
    TArray<uint8>B;if(!W.Finish(B)){E=TEXT("Registry canonical size exceeded");return false;}R->Hash=VoxelBinary::Hash(B);
    Published=R;E.Reset();return true;
}
