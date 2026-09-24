#include "Voxel/VoxelMaterialBuilder.h"
#include "Voxel/VoxelEditorAssetIO.h"
#include "Engine/Texture2DArray.h"
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialExpressionTextureSampleParameter2DArray.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionTime.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionVertexColor.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionDepthFade.h"
#include "Materials/MaterialExpressionFresnel.h"
namespace
{
    template<class T> T* Node(UMaterial* M,int32 X,int32 Y)
    {return CastChecked<T>(UMaterialEditingLibrary::CreateMaterialExpression(M,T::StaticClass(),X,Y));}
    void AddInput(UMaterialExpressionCustom* C,const TCHAR* Name,UMaterialExpression* E,int32 Output=0)
    {FCustomInput I;I.InputName=Name;I.Input.Connect(Output,E);C->Inputs.Add(I);}
}
UMaterial* FVoxelMaterialBuilder::BuildMaster(const FString& Path,EVoxelRenderGroup Group,UTexture2DArray* Array,FString& Error)
{
    if(!Array){Error=TEXT("Material requires an array texture");return nullptr;}
    UMaterial* M=Cast<UMaterial>(FVoxelEditorAssetIO::LoadOrCreate(UMaterial::StaticClass(),Path,Error));
    if(!M)return nullptr;
    M->PreEditChange(nullptr);UMaterialEditingLibrary::DeleteAllMaterialExpressions(M);
    M->MaterialDomain=MD_Surface;M->SetShadingModel(MSM_DefaultLit);
    M->TwoSided=Group==EVoxelRenderGroup::Foliage||Group==EVoxelRenderGroup::Masked||Group==EVoxelRenderGroup::Water;
    const bool Trans=Group==EVoxelRenderGroup::Water||Group==EVoxelRenderGroup::Translucent;
    M->BlendMode=Trans?BLEND_Translucent:(M->TwoSided?BLEND_Masked:BLEND_Opaque);
    M->OpacityMaskClipValue=0.4f;
    auto* UV=Node<UMaterialExpressionTextureCoordinate>(M,-1000,0);UV->CoordinateIndex=0;
    auto* Layer=Node<UMaterialExpressionTextureCoordinate>(M,-1000,160);Layer->CoordinateIndex=1;
    auto* FPS=Node<UMaterialExpressionTextureCoordinate>(M,-1000,300);FPS->CoordinateIndex=2;
    auto* Time=Node<UMaterialExpressionTime>(M,-1000,440);
    auto* Pos=Node<UMaterialExpressionWorldPosition>(M,-1000,580);
    auto* Color=Node<UMaterialExpressionVertexColor>(M,-1000,720);
    auto* Coord=Node<UMaterialExpressionCustom>(M,-650,0);Coord->OutputType=CMOT_Float3;
    Coord->Description=TEXT("Voxel array layer, animation and water UV");
    Coord->Code=Group==EVoxelRenderGroup::Water?
        TEXT("float2 p=UV+float2(T*0.035,T*0.019); return float3(p,L.x+fmod(floor(T*F.x),max(1.0,L.y)));"):
        TEXT("return float3(UV,L.x+fmod(floor(T*F.x),max(1.0,L.y)));");
    AddInput(Coord,TEXT("UV"),UV);AddInput(Coord,TEXT("L"),Layer);AddInput(Coord,TEXT("F"),FPS);AddInput(Coord,TEXT("T"),Time);
    auto* Sample=Node<UMaterialExpressionTextureSampleParameter2DArray>(M,-350,0);
    Sample->ParameterName=TEXT("VoxelArray");Sample->Texture=Array;Sample->SamplerType=SAMPLERTYPE_Color;
    Sample->Coordinates.Connect(0,Coord);
    UMaterialExpression* BaseColor=Sample;
    if(Group==EVoxelRenderGroup::Opaque)
    {
        auto* Stylized=Node<UMaterialExpressionCustom>(M,-100,0);
        Stylized->OutputType=CMOT_Float3;
        Stylized->Description=TEXT("World palette tint and restrained per-cell color variation");
		Stylized->Code=TEXT("float3 cell=floor(P/25.0); float h=frac(sin(dot(cell,float3(17.13,43.71,11.97)))*43758.5453); return saturate(Tex.rgb*Tint.rgb*lerp(0.96,1.04,h));");
        AddInput(Stylized,TEXT("Tex"),Sample);
        AddInput(Stylized,TEXT("Tint"),Color);
        AddInput(Stylized,TEXT("P"),Pos);
        BaseColor=Stylized;
    }
    bool OK=UMaterialEditingLibrary::ConnectMaterialProperty(BaseColor,TEXT(""),MP_BaseColor);
    auto* Rough=Node<UMaterialExpressionConstant>(M,-300,360);Rough->R=Trans?0.12f:0.9f;
    OK&=UMaterialEditingLibrary::ConnectMaterialProperty(Rough,TEXT(""),MP_Roughness);
    if(Group==EVoxelRenderGroup::Opaque)
    {
        auto* Specular=Node<UMaterialExpressionConstant>(M,-300,470);Specular->R=0.16f;
        OK&=UMaterialEditingLibrary::ConnectMaterialProperty(Specular,TEXT(""),MP_Specular);
    }
    if(M->BlendMode==BLEND_Masked)OK&=UMaterialEditingLibrary::ConnectMaterialProperty(Sample,TEXT("A"),MP_OpacityMask);
    if(Group==EVoxelRenderGroup::Emissive)OK&=UMaterialEditingLibrary::ConnectMaterialProperty(Sample,TEXT("RGB"),MP_EmissiveColor);
    if(Trans)
    {
        auto* Fade=Node<UMaterialExpressionDepthFade>(M,-600,850);Fade->FadeDistanceDefault=80.f;Fade->OpacityDefault=1.f;
        auto* Fresnel=Node<UMaterialExpressionFresnel>(M,-600,1050);
        auto* Opacity=Node<UMaterialExpressionCustom>(M,-250,850);Opacity->OutputType=CMOT_Float1;
        Opacity->Code=Group==EVoxelRenderGroup::Water?TEXT("return saturate((0.35+0.45*F)*D);"):TEXT("return saturate(A*0.5);");
        AddInput(Opacity,TEXT("A"),Sample,4);AddInput(Opacity,TEXT("D"),Fade);AddInput(Opacity,TEXT("F"),Fresnel);
        OK&=UMaterialEditingLibrary::ConnectMaterialProperty(Opacity,TEXT(""),MP_Opacity);
    }
    if(Group==EVoxelRenderGroup::Foliage||Group==EVoxelRenderGroup::Water)
    {
        auto* Wind=Node<UMaterialExpressionCustom>(M,-250,600);Wind->OutputType=CMOT_Float3;
        Wind->Code=Group==EVoxelRenderGroup::Foliage?
            TEXT("float a=sin(T*1.8+P.x*0.008+P.y*0.006)*Mask*3.0;return float3(a,a*0.3,0);"):
            TEXT("return float3(0,0,sin(T*1.5+P.x*0.012)*sin(T*0.8+P.y*0.011)*1.0);");
        AddInput(Wind,TEXT("P"),Pos);AddInput(Wind,TEXT("T"),Time);AddInput(Wind,TEXT("Mask"),Color,1);
        OK&=UMaterialEditingLibrary::ConnectMaterialProperty(Wind,TEXT(""),MP_WorldPositionOffset);
    }
    if(!OK){Error=TEXT("Material graph connection failed: ")+Path;return nullptr;}
    M->PostEditChange();UMaterialEditingLibrary::RecompileMaterial(M);
    return FVoxelEditorAssetIO::Save(M,Error)?M:nullptr;
}
UMaterialInstanceConstant* FVoxelMaterialBuilder::BuildInstance(const FString& Path,UMaterial* Parent,UTexture2DArray* Array,FString& Error)
{
    if(!Parent||!Array){Error=TEXT("Missing material parent or texture array");return nullptr;}
    auto* MI=Cast<UMaterialInstanceConstant>(FVoxelEditorAssetIO::LoadOrCreate(UMaterialInstanceConstant::StaticClass(),Path,Error));
    if(!MI)return nullptr;
    UMaterialEditingLibrary::SetMaterialInstanceParent(MI,Parent);
    MI->SetTextureParameterValueEditorOnly(FMaterialParameterInfo(TEXT("VoxelArray")),Array);
    UMaterialEditingLibrary::UpdateMaterialInstance(MI);
    return FVoxelEditorAssetIO::Save(MI,Error)?MI:nullptr;
}
