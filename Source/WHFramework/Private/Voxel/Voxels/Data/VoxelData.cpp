#include "Voxel/Voxels/Data/VoxelData.h"
#include "Ability/PickUp/AbilityPickUpVoxel.h"
bool FVoxelBakedFaceRef::ToRuntime(FVoxelRuntimeFaceRef&O)const
{
    if(Bank<0||Bank>65535||Layer<0||Layer>65535||Frames<1||Frames>256||FPS<0||FPS>60||Layer+Frames>65536)return false;
    O={uint16(Bank),uint16(Layer),uint16(Frames),uint16(FPS)};return true;
}
UVoxelData::UVoxelData()
{
    Type=TEXT("Voxel");MaxCount=64;MaxLevel=0;PickUpClass=AAbilityPickUpVoxel::StaticClass();
}
void UVoxelData::OnInitialize_Implementation(){Super::OnInitialize_Implementation();}
void UVoxelData::OnReset_Implementation(){Super::OnReset_Implementation();}
bool UVoxelData::ValidateDefinition(bool Render,FString&E)const
{
    if(!bRegisterBlock){E.Reset();return true;}
    const FString N=BlockName.ToString();
    if(BlockName.IsNone()||!N.Contains(TEXT(":"))||N!=N.ToLower()||N==TEXT("core:air")){E=TEXT("Invalid stable block name");return false;}
    for(TCHAR C:N)if(!((C>='a'&&C<='z')||(C>='0'&&C<='9')||C==':'||C=='_'||C=='/'||C=='.'||C=='-')){E=TEXT("Block name must be lowercase ASCII");return false;}
    if(uint8(Shape)>uint8(EVoxelShapeKind::Fluid)||uint8(RenderGroup)>uint8(EVoxelRenderGroup::Emissive)||
       BreakMilliseconds<0||BreakMilliseconds>600000||DropCount<0||DropCount>64||EntityKind<0||EntityKind>65535)
    {E=TEXT("Invalid voxel definition range");return false;}
    if((Shape==EVoxelShapeKind::CrossPlant||Shape==EVoxelShapeKind::Fluid||Shape==EVoxelShapeKind::Torch||Shape==EVoxelShapeKind::Ladder)&&bSolid)
    {E=TEXT("This non-cube shape must use bSolid=false");return false;}
    if((EntityKind==100&&(EntityVariant<1||EntityVariant>2))||(EntityKind!=100&&EntityVariant!=0)){E=TEXT("Invalid entity variant");return false;}
    if(EntityKind!=0&&EntityKind!=1&&EntityKind!=2&&EntityKind!=100&&EntityKind!=101){E=TEXT("Unsupported voxel entity kind");return false;}
    for(int32 Half=0;Half<(Shape==EVoxelShapeKind::Door?2:1);++Half)
    {
        const FVoxelFaceMaterialSet& Set=Half?UpperFaceMaterials:FaceMaterials;
        for(uint8 Face=0;Face<6;++Face)
        {
            const FVoxelFaceTexture& Material=Set.Get(Face);
            if(Material.ShadingMode!=EVoxelFaceShadingMode::PaletteColor)continue;
            const FLinearColor& Color=Material.PaletteColor;
            if(RenderGroup!=EVoxelRenderGroup::Opaque||Material.FrameCount!=1||Material.FramesPerSecond!=0||
                !FMath::IsFinite(Color.R)||!FMath::IsFinite(Color.G)||!FMath::IsFinite(Color.B)||
                Color.R<0.f||Color.R>1.f||Color.G<0.f||Color.G>1.f||Color.B<0.f||Color.B>1.f)
            {E=TEXT("Palette faces require an opaque static block and finite unit-range color");return false;}
        }
    }
    if(Render)
    {
        if(BakeVersion!=3||BakedFaces.Num()!=6||(Shape==EVoxelShapeKind::Door&&BakedUpperFaces.Num()!=6))
        {E=TEXT("Missing texture-array bake version 3");return false;}
        FVoxelRuntimeFaceRef R;for(const auto&F:BakedFaces)if(!F.ToRuntime(R)){E=TEXT("Invalid baked face");return false;}
        for(const auto&F:BakedUpperFaces)if(!F.ToRuntime(R)){E=TEXT("Invalid baked upper face");return false;}
    }
    E.Reset();return true;
}
