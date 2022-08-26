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
	BlockScale = 1;
	OffsetScale = 1;
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
			BlockScale = 1.f;
			OffsetScale = 1.f;
			CenterOffset = FVector(0.5f);
			switch(Transparency)
			{
				case EVoxelTransparency::Solid:
				case EVoxelTransparency::SemiTransparent:
				{
					SetCollisionProfileName(TEXT("SolidVoxel"));
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
			BlockScale = 0.3f;
			OffsetScale = 0;
			SetCollisionEnabled(false);
			break;
		}
		case EVoxelMeshNature::Vitality:
		{
			BlockScale = 1.f;
			OffsetScale = 1.f;
			CenterOffset = FVector(0.f, 0.f, 0.5f);
			SetCollisionEnabled(false);
			break;
		}
	}
}

void UVoxelMeshComponent::BuildVoxel(const FVoxelItem& InVoxelItem)
{
	const UVoxelData& voxelData = InVoxelItem.GetVoxelData();
	switch(voxelData.MeshType)
	{
		case EVoxelMeshType::Custom:
		{
			TArray<FVector> meshVertices, meshNormals;
			voxelData.GetMeshData(InVoxelItem, meshVertices, meshNormals);
			for (int i = 0; i < meshVertices.Num(); i++)
			{
				if (i > 0 && (i + 1) % 4 == 0)
				{
					FVector vertices[4];
					for (int j = 0; j < 4; j++)
					{
						vertices[j] = meshVertices[i - (3 - j)];
					}
					BuildFace(InVoxelItem, vertices, i / 4, meshNormals[i / 4]);
				}
			}
			break;
		}
		case EVoxelMeshType::Cube:
		{
			DIRECTION_ITERATOR(Iter,
				if (!GetOwnerChunk() || !GetOwnerChunk()->CheckVoxelAdjacent(InVoxelItem.Index, Iter))
				{
					BuildFace(InVoxelItem, Iter);
				}
			)
			break;
		}
	}
}

void UVoxelMeshComponent::CreateVoxel(const FVoxelItem& InVoxelItem)
{
	if(InVoxelItem.IsValid())
	{
		UVoxelData& voxelData = InVoxelItem.GetVoxelData();
		const FVector range = voxelData.GetRange();
		Transparency = voxelData.Transparency;
		CenterOffset = FVector(0.f, 0.f, -range.Z * 0.5f + 0.5f);
		INDEX_ITERATOR(partIndex, range,
			UVoxelData& partData = voxelData.GetPartData(partIndex);
			if(partData.IsValid())
			{
				FVoxelItem partItem;
				partItem.ID = partData.GetPrimaryAssetId();
				partItem.Index = partIndex;
				BuildVoxel(partItem);
			}
		)
		CreateMesh(0, false);
	}
	else
	{
		ClearMesh(0);
	}
}

void UVoxelMeshComponent::CreateMesh(int InSectionIndex /*= 0*/, bool bHasCollider /*= true*/)
{
	if (HasData())
	{
		SetCollisionEnabled(true);
		CreateMeshSection(InSectionIndex, Vertices, Triangles, Normals, UVs, VertexColors, Tangents, bHasCollider);
		switch (MeshNature)
		{
			case EVoxelMeshNature::Chunk:
			case EVoxelMeshNature::PickUp:
			case EVoxelMeshNature::Entity:
			case EVoxelMeshNature::Vitality:
			{
				SetMaterial(InSectionIndex, UVoxelModuleBPLibrary::GetWorldData().GetChunkMaterial(Transparency).Material);
				break;
			}
			case EVoxelMeshNature::Preview:
			{
				SetMaterial(InSectionIndex, UVoxelModuleBPLibrary::GetWorldData().GetChunkMaterial(Transparency).UnlitMaterial);
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

void UVoxelMeshComponent::ClearMesh(int InSectionIndex /*= 0*/)
{
	if (GetNumSections() > 0)
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

void UVoxelMeshComponent::BuildFace(const FVoxelItem& InVoxelItem, EDirection InFacing)
{
	FVector vertices[4];
	switch (InFacing)
	{
		case EDirection::Forward:
		{
			vertices[0] = FVector(0.5f, 0.5f, -0.5f);
			vertices[1] = FVector(0.5f, 0.5f, 0.5f);
			vertices[2] = FVector(0.5f, -0.5f, 0.5f);
			vertices[3] = FVector(0.5f, -0.5f, -0.5f);
			break;
		}
		case EDirection::Right:
		{
			vertices[0] = FVector(-0.5f, 0.5f, -0.5f);
			vertices[1] = FVector(-0.5f, 0.5f, 0.5f);
			vertices[2] = FVector(0.5f, 0.5f, 0.5f);
			vertices[3] = FVector(0.5f, 0.5f, -0.5f);
			break;
		}
		case EDirection::Backward:
		{
			vertices[0] = FVector(-0.5f, -0.5f, -0.5f);
			vertices[1] = FVector(-0.5f, -0.5f, 0.5f);
			vertices[2] = FVector(-0.5f, 0.5f, 0.5f);
			vertices[3] = FVector(-0.5f, 0.5f, -0.5f);
			break;
		}
		case EDirection::Left:
		{
			vertices[0] = FVector(0.5f, -0.5f, -0.5f);
			vertices[1] = FVector(0.5f, -0.5f, 0.5f);
			vertices[2] = FVector(-0.5f, -0.5f, 0.5f);
			vertices[3] = FVector(-0.5f, -0.5f, -0.5f);
			break;
		}
		case EDirection::Up:
		{
			vertices[0] = FVector(-0.5f, -0.5f, 0.5f);
			vertices[1] = FVector(0.5f, -0.5f, 0.5f);
			vertices[2] = FVector(0.5f, 0.5f, 0.5f);
			vertices[3] = FVector(-0.5f, 0.5f, 0.5f);
			break;
		}
		case EDirection::Down:
		{
			vertices[0] = FVector(-0.5f, 0.5f, -0.5f);
			vertices[1] = FVector(0.5f, 0.5f, -0.5f);
			vertices[2] = FVector(0.5f, -0.5f, -0.5f);
			vertices[3] = FVector(-0.5f, -0.5f, -0.5f);
			break;
		}
	}

	BuildFace(InVoxelItem, vertices, (int32)InFacing, UMathBPLibrary::DirectionToVector(InFacing, InVoxelItem.Angle));
}

void UVoxelMeshComponent::BuildFace(const FVoxelItem& InVoxelItem, FVector InVertices[4], int InFaceIndex, FVector InNormal)
{
	const int32 verNum = Vertices.Num();
	const UVoxelData& voxelData = InVoxelItem.GetVoxelData();
	FVector scale, offset;
	voxelData.GetMeshData(InVoxelItem, scale, offset);
	scale = UMathBPLibrary::RotatorVector(scale, InVoxelItem.Angle, false, true);
	offset = CenterOffset + UMathBPLibrary::RotatorVector(offset, InVoxelItem.Angle) * OffsetScale;
	FVector2D uvCorner, uvSpan;
	voxelData.GetUVData(InVoxelItem, InFaceIndex, UVoxelModuleBPLibrary::GetWorldData().GetChunkMaterial(voxelData.Transparency).BlockUVSize, uvCorner, uvSpan);

	for (int32 i = 0; i < 4; i++)
	{
		Vertices.Add((InVoxelItem.Index.ToVector() + UMathBPLibrary::RotatorVector(InVertices[i], InVoxelItem.Angle) * scale + offset) * UVoxelModuleBPLibrary::GetWorldData().BlockSize * BlockScale);
	}

	UVs.Add(FVector2D(uvCorner.X, uvCorner.Y + uvSpan.Y));
	UVs.Add(uvCorner);
	UVs.Add(FVector2D(uvCorner.X + uvSpan.X, uvCorner.Y));
	UVs.Add(FVector2D(uvCorner.X + uvSpan.X, uvCorner.Y + uvSpan.Y));

	Triangles.Add(verNum + 1);
	Triangles.Add(verNum + 0);
	Triangles.Add(verNum + 2);
	Triangles.Add(verNum + 3);
	Triangles.Add(verNum + 2);
	Triangles.Add(verNum + 0);

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
	bool ReturnValue = false;
	for(int32 i = 0; i < GetNumSections(); i++)
	{
		if(const auto MeshSection = GetProcMeshSection(i))
		{
			if(!MeshSection->ProcVertexBuffer.IsEmpty())
			{
				ReturnValue = true;
			}
		}
	}
	return ReturnValue;
}

AVoxelChunk* UVoxelMeshComponent::GetOwnerChunk() const
{
	return Cast<AVoxelChunk>(GetOwner());
}

void UVoxelMeshComponent::SetCollisionEnabled(ECollisionEnabled::Type NewType)
{
	Super::SetCollisionEnabled(NewType);
}

void UVoxelMeshComponent::SetCollisionEnabled(bool bNewEnable)
{
	switch(Transparency)
	{
		case EVoxelTransparency::Solid:
		case EVoxelTransparency::SemiTransparent:
		{
			SetCollisionEnabled(bNewEnable ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
			break;
		}
		case EVoxelTransparency::Transparent:
		{
			SetCollisionEnabled(bNewEnable ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
			break;
		}
	}
}
