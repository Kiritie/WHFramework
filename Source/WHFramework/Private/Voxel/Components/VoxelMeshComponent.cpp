// Fill out your copyright notice in the Description page of Project Settings.


#include "Voxel/Components/VoxelMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Math/MathBPLibrary.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/VoxelModuleBPLibrary.h"
#include "Voxel/Datas/VoxelData.h"
#include "Voxel/Chunks/VoxelChunk.h"

UVoxelMeshComponent::UVoxelMeshComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bUseAsyncCooking = true;
	OffsetScale = FVector::OneVector;
	CenterOffset = FVector(0.5f);
	MeshNature = EVoxelMeshNature::Chunk;
	Transparency = EVoxelTransparency::Solid;
	Vertices = TArray<FVector>();
	Triangles = TArray<int32>();
	Normals = TArray<FVector>();
	UVs = TArray<FVector2D>();
	VertexColors = TArray<FColor>();
	Tangents = TArray<FProcMeshTangent>();
}

void UVoxelMeshComponent::Initialize(EVoxelMeshNature InMeshNature, EVoxelTransparency InTransparency /*= EVoxelTransparency::Solid*/)
{
	MeshNature = InMeshNature;
	Transparency = InTransparency;
	switch (MeshNature)
	{
		case EVoxelMeshNature::Chunk:
		{
			OffsetScale = FVector::OneVector;
			CenterOffset = FVector(0.5f);
			switch(Transparency)
			{
				case EVoxelTransparency::Solid:
				{
					SetCollisionProfileName(TEXT("SolidVoxel"));
					break;
				}
				case EVoxelTransparency::SemiTransparent:
				{
					SetCollisionProfileName(TEXT("SemiVoxel"));
					break;
				}
				case EVoxelTransparency::Transparent:
				{
					SetCollisionProfileName(TEXT("TransVoxel"));
					break;
				}
			}
			SetCollisionEnabled(true);
			break;
		}
		case EVoxelMeshNature::PickUp:
		case EVoxelMeshNature::Entity:
		case EVoxelMeshNature::Preview:
		{
			OffsetScale = FVector::ZeroVector;
			CenterOffset = FVector(0.5f);
			SetCollisionEnabled(false);
			break;
		}
		case EVoxelMeshNature::Vitality:
		{
			OffsetScale = FVector::OneVector;
			CenterOffset = FVector(0.f, 0.f, 0.5f);
			SetCollisionEnabled(false);
			break;
		}
	}
}

void UVoxelMeshComponent::BuildVoxel(const FVoxelItem& InVoxelItem)
{
	const UVoxelData& VoxelData = InVoxelItem.GetVoxelData();
	const FVoxelMeshData& MeshData = VoxelData.GetMeshData(InVoxelItem);
	if(MeshData.bCustomMesh)
	{
		for (int i = 0; i < MeshData.MeshVertices.Num(); i++)
		{
			if (i > 0 && (i + 1) % 4 == 0)
			{
				FVector Vers[4];
				for (int j = 0; j < 4; j++)
				{
					Vers[j] = MeshData.MeshVertices[i - (3 - j)];
				}
				BuildFace(InVoxelItem, Vers, i / 4, MeshData.MeshNormals[i / 4].GetSafeNormal());
			}
		}
	}
	else
	{
		ITER_DIRECTION(Iter,
			if (!GetOwnerChunk() || !GetOwnerChunk()->CheckVoxelAdjacent(InVoxelItem, Iter))
			{
				BuildFace(InVoxelItem, Iter);
			}
		)
	}
}

void UVoxelMeshComponent::CreateVoxel(const FVoxelItem& InVoxelItem)
{
	if(InVoxelItem.IsValid())
	{
		UVoxelData& VoxelData = InVoxelItem.GetVoxelData();
		const FVector Range = VoxelData.GetRange();
		Transparency = VoxelData.Transparency;
		CenterOffset = FVector(0.f, 0.f, -Range.Z * 0.5f + 0.5f);
		ITER_INDEX(PartIndex, Range, false,
			UVoxelData& PartData = VoxelData.GetPartData(PartIndex);
			if(PartData.IsValid())
			{
				FVoxelItem PartItem;
				PartItem.ID = PartData.GetPrimaryAssetId();
				PartItem.Index = PartIndex;
				BuildVoxel(PartItem);
			}
		)
		CreateMesh(0, false);
	}
	else
	{
		ClearMesh(0);
	}
}

void UVoxelMeshComponent::CreateMesh(int32 InSectionIndex /*= 0*/, bool bHasCollision /*= true*/)
{
	if (HasData())
	{
		SetCollisionEnabled(true);
		CreateMeshSection(InSectionIndex, Vertices, Triangles, Normals, UVs, VertexColors, Tangents, bHasCollision);
		switch (MeshNature)
		{
			case EVoxelMeshNature::Chunk:
			case EVoxelMeshNature::PickUp:
			case EVoxelMeshNature::Entity:
			case EVoxelMeshNature::Vitality:
			{
				SetMaterial(InSectionIndex, AVoxelModule::Get()->GetWorldData().RenderDatas[Transparency].MaterialInst);
				break;
			}
			case EVoxelMeshNature::Preview:
			{
				SetMaterial(InSectionIndex, AVoxelModule::Get()->GetWorldData().RenderDatas[Transparency].UnlitMaterialInst);
				break;
			}
		}
		ClearData();
	}
	else
	{
		ClearMesh(InSectionIndex);
	}
}

void UVoxelMeshComponent::ClearMesh(int32 InSectionIndex /*= 0*/)
{
	if (GetNumSections() > InSectionIndex)
	{
		ClearMeshSection(InSectionIndex);
	}
	if (!HasMesh())
	{
		SetCollisionEnabled(false);
	}
	ClearData();
}

void UVoxelMeshComponent::ClearData()
{
	Vertices.Empty();
	Triangles.Empty();
	Normals.Empty();
	UVs.Empty();
	VertexColors.Empty();
	Tangents.Empty();
}

void UVoxelMeshComponent::SetCollisionEnabled(ECollisionEnabled::Type NewType)
{
	Super::SetCollisionEnabled(NewType);
}

void UVoxelMeshComponent::SetCollisionEnabled(bool bEnable)
{
	switch(Transparency)
	{
		case EVoxelTransparency::Solid:
		case EVoxelTransparency::SemiTransparent:
		{
			SetCollisionEnabled(bEnable ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
			break;
		}
		case EVoxelTransparency::Transparent:
		{
			SetCollisionEnabled(bEnable ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
			break;
		}
	}
}

void UVoxelMeshComponent::BuildFace(const FVoxelItem& InVoxelItem, EDirection InFacing)
{
	FVector Vers[4];
	switch (InFacing)
	{
		case EDirection::Forward:
		{
			Vers[0] = FVector(0.5f, 0.5f, -0.5f);
			Vers[1] = FVector(0.5f, 0.5f, 0.5f);
			Vers[2] = FVector(0.5f, -0.5f, 0.5f);
			Vers[3] = FVector(0.5f, -0.5f, -0.5f);
			break;
		}
		case EDirection::Right:
		{
			Vers[0] = FVector(-0.5f, 0.5f, -0.5f);
			Vers[1] = FVector(-0.5f, 0.5f, 0.5f);
			Vers[2] = FVector(0.5f, 0.5f, 0.5f);
			Vers[3] = FVector(0.5f, 0.5f, -0.5f);
			break;
		}
		case EDirection::Backward:
		{
			Vers[0] = FVector(-0.5f, -0.5f, -0.5f);
			Vers[1] = FVector(-0.5f, -0.5f, 0.5f);
			Vers[2] = FVector(-0.5f, 0.5f, 0.5f);
			Vers[3] = FVector(-0.5f, 0.5f, -0.5f);
			break;
		}
		case EDirection::Left:
		{
			Vers[0] = FVector(0.5f, -0.5f, -0.5f);
			Vers[1] = FVector(0.5f, -0.5f, 0.5f);
			Vers[2] = FVector(-0.5f, -0.5f, 0.5f);
			Vers[3] = FVector(-0.5f, -0.5f, -0.5f);
			break;
		}
		case EDirection::Up:
		{
			Vers[0] = FVector(-0.5f, -0.5f, 0.5f);
			Vers[1] = FVector(0.5f, -0.5f, 0.5f);
			Vers[2] = FVector(0.5f, 0.5f, 0.5f);
			Vers[3] = FVector(-0.5f, 0.5f, 0.5f);
			break;
		}
		case EDirection::Down:
		{
			Vers[0] = FVector(-0.5f, 0.5f, -0.5f);
			Vers[1] = FVector(0.5f, 0.5f, -0.5f);
			Vers[2] = FVector(0.5f, -0.5f, -0.5f);
			Vers[3] = FVector(-0.5f, -0.5f, -0.5f);
			break;
		}
		default: break;
	}

	BuildFace(InVoxelItem, Vers, (int32)InFacing, UMathBPLibrary::DirectionToVector(InFacing, InVoxelItem.Angle));
}

void UVoxelMeshComponent::BuildFace(const FVoxelItem& InVoxelItem, FVector InVertices[4], int32 InFaceIndex, FVector InNormal)
{
	const int32 VerNum = Vertices.Num();
	const UVoxelData& VoxelData = InVoxelItem.GetVoxelData();
	const FVoxelMeshData& MeshData = VoxelData.GetMeshData(InVoxelItem);
	const FVoxelMeshUVData& MeshUVData = MeshData.MeshUVDatas[InFaceIndex];
	const FVoxelRenderData& RenderData = AVoxelModule::Get()->GetWorldData().RenderDatas[VoxelData.Transparency];
	const FVector Scale = UMathBPLibrary::RotatorVector(MeshData.MeshScale, InVoxelItem.Angle, false, true);
	const FVector Offset = CenterOffset + UMathBPLibrary::RotatorVector(MeshData.MeshOffset, InVoxelItem.Angle) * OffsetScale;
	const FVector2D UVSize = FVector2D(RenderData.PixelSize / RenderData.TextureSize.X, RenderData.PixelSize / RenderData.TextureSize.Y);
	const FVector2D UVCorner = FVector2D((MeshUVData.UVCorner.X + MeshUVData.UVOffset.X) * UVSize.X,
		(1.f / UVSize.Y - (RenderData.TextureSize.Y / RenderData.PixelSize - (-MeshUVData.UVCorner.Y + MeshUVData.UVOffset.Y) - 1.f) - MeshUVData.UVSpan.Y) * UVSize.Y);
	const FVector2D UVSpan = FVector2D(MeshUVData.UVSpan.X * UVSize.X, MeshUVData.UVSpan.Y * UVSize.Y);

	for (int32 i = 0; i < 4; i++)
	{
		Vertices.Add((InVoxelItem.Index.ToVector() + UMathBPLibrary::RotatorVector(InVertices[i], InVoxelItem.Angle) * Scale + Offset) * AVoxelModule::Get()->GetWorldData().BlockSize);
	}

	UVs.Add(FVector2D(UVCorner.X, UVCorner.Y + UVSpan.Y));
	UVs.Add(UVCorner);
	UVs.Add(FVector2D(UVCorner.X + UVSpan.X, UVCorner.Y));
	UVs.Add(FVector2D(UVCorner.X + UVSpan.X, UVCorner.Y + UVSpan.Y));

	Triangles.Add(VerNum + 1);
	Triangles.Add(VerNum + 0);
	Triangles.Add(VerNum + 2);
	Triangles.Add(VerNum + 3);
	Triangles.Add(VerNum + 2);
	Triangles.Add(VerNum + 0);

	Normals.Add(InNormal);
	Normals.Add(InNormal);
	Normals.Add(InNormal);
	Normals.Add(InNormal);
}

bool UVoxelMeshComponent::HasData() const
{
	return Vertices.Num() > 0;
}

bool UVoxelMeshComponent::HasMesh()
{
	for(int32 i = 0; i < GetNumSections(); i++)
	{
		if(const auto MeshSection = GetProcMeshSection(i))
		{
			if(!MeshSection->ProcVertexBuffer.IsEmpty())
			{
				return true;
			}
		}
	}
	return false;
}

AVoxelChunk* UVoxelMeshComponent::GetOwnerChunk() const
{
	return Cast<AVoxelChunk>(GetOwner());
}
