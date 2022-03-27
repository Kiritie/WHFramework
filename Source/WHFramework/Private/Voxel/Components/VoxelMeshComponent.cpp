// Fill out your copyright notice in the Description page of Project Settings.


#include "Voxel/Components/VoxelMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/VoxelModuleBPLibrary.h"
#include "Voxel/Voxels/VoxelAssetBase.h"
#include "Voxel/Chunks/VoxelChunk.h"

UVoxelMeshComponent::UVoxelMeshComponent(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	bUseAsyncCooking = true;
	BlockScale = 1;
	OffsetScale = 1;
	CenterOffset = FVector(0.5f);
	MeshType = EVoxelMeshType::Chunk;
	Transparency = ETransparency::Solid;
	Vertices = TArray<FVector>();
	Triangles = TArray<int32>();
	Normals = TArray<FVector>();
	UVs = TArray<FVector2D>();
	VertexColors = TArray<FColor>();
	Tangents = TArray<FProcMeshTangent>();
}

void UVoxelMeshComponent::Initialize(EVoxelMeshType InMeshType, ETransparency InTransparency /*= ETransparency::Solid*/)
{
	MeshType = InMeshType;
	Transparency = InTransparency;
	switch (MeshType)
	{
		case EVoxelMeshType::Chunk:
		{
			BlockScale = 1.f;
			OffsetScale = 1.f;
			CenterOffset = FVector(0.5f);
			switch (Transparency)
			{
				case ETransparency::Solid:
				case ETransparency::SemiTransparent:
				{
					SetCollisionProfileName(TEXT("DW_SolidVoxel"));
					break;
				}
				case ETransparency::Transparent:
				{
					SetCollisionProfileName(TEXT("DW_TransVoxel"));
					break;
				}
			}
			break;
		}
		case EVoxelMeshType::PickUp:
		{
			BlockScale = 0.3f;
			OffsetScale = 0;
			CenterOffset = FVector::ZeroVector;
			break;
		}
		case EVoxelMeshType::PreviewItem:
		{
			BlockScale = 0.3f;
			OffsetScale = 0;
			CenterOffset = FVector::ZeroVector;
			break;
		}
		case EVoxelMeshType::VitalityVoxel:
		{
			BlockScale = 1.f;
			OffsetScale = 0;
			CenterOffset = FVector::ZeroVector;
			break;
		}
	}
}

void UVoxelMeshComponent::BuildVoxel(const FVoxelItem& InVoxelItem)
{
	const UVoxelAssetBase& voxelData = InVoxelItem.GetData<UVoxelAssetBase>();
	if(voxelData.bCustomMesh)
	{
		TArray<FVector> meshVertices, meshNormals;
		if (voxelData.GetMeshDatas(meshVertices, meshNormals, InVoxelItem.Rotation, InVoxelItem.Scale))
		{
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
		}
	}
	else
	{
		for (int i = 0; i < 6; i++)
		{
			if (!GetOwnerChunk() || GetOwnerChunk()->CheckAdjacent(InVoxelItem, (EDirection)i))
			{
				BuildFace(InVoxelItem, (EFacing)i);
			}
		}
	}
	if (MeshType == EVoxelMeshType::PickUp || MeshType == EVoxelMeshType::PreviewItem || MeshType == EVoxelMeshType::VitalityVoxel)
	{
		Transparency = voxelData.Transparency;
	}
}

void UVoxelMeshComponent::CreateMesh(int InSectionIndex /*= 0*/, bool bHasCollider /*= true*/)
{
	if (Vertices.Num() > 0)
	{
		CreateMeshSection(InSectionIndex, Vertices, Triangles, Normals, UVs, VertexColors, Tangents, bHasCollider);
		UMaterialInterface* material = nullptr;
		switch (MeshType)
		{
			case EVoxelMeshType::Chunk:
			case EVoxelMeshType::PickUp:
			case EVoxelMeshType::VitalityVoxel:
			{
				material = AVoxelModule::GetWorldData().GetChunkMaterial(Transparency).Material;
				break;
			}
			case EVoxelMeshType::PreviewItem:
			{
				material = UMaterialInstanceDynamic::Create(AVoxelModule::GetWorldData().GetChunkMaterial(Transparency).Material, this);
				Cast<UMaterialInstanceDynamic>(material)->SetScalarParameterValue(TEXT("Emissive"), 1);
				break;
			}
		}
		SetMaterial(InSectionIndex, material);
		ClearData();
	}
	else
	{
		ClearMesh(0);
	}
}

void UVoxelMeshComponent::ClearMesh(int InSectionIndex /*= 0*/)
{
	if (GetNumSections() > 0)
	{
		if (InSectionIndex >= 0 && InSectionIndex < GetNumSections())
		{
			ClearMeshSection(InSectionIndex);
		}
		else
		{
			ClearAllMeshSections();
		}
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

void UVoxelMeshComponent::BuildFace(const FVoxelItem& InVoxelItem, EFacing InFacing)
{
	FVector vertices[4];
	FVector range = InVoxelItem.GetData<UVoxelAssetBase>().GetFinalRange(InVoxelItem.Rotation, InVoxelItem.Scale);

	switch (InFacing)
	{
		case EFacing::Up:
		{
			vertices[0] = FVector(-range.X, -range.Y, range.Z) * 0.5f;
			vertices[1] = FVector(range.X, -range.Y, range.Z) * 0.5f;
			vertices[2] = FVector(range.X, range.Y, range.Z) * 0.5f;
			vertices[3] = FVector(-range.X, range.Y, range.Z) * 0.5f;
			break;
		}
		case EFacing::Down:
		{
			vertices[0] = FVector(-range.X, range.Y, -range.Z) * 0.5f;
			vertices[1] = FVector(range.X, range.Y, -range.Z) * 0.5f;
			vertices[2] = FVector(range.X, -range.Y, -range.Z) * 0.5f;
			vertices[3] = FVector(-range.X, -range.Y, -range.Z) * 0.5f;
			break;
		}
		case EFacing::Forward:
		{
			vertices[0] = FVector(range.X, range.Y, -range.Z) * 0.5f;
			vertices[1] = FVector(range.X, range.Y, range.Z) * 0.5f;
			vertices[2] = FVector(range.X, -range.Y, range.Z) * 0.5f;
			vertices[3] = FVector(range.X, -range.Y, -range.Z) * 0.5f;
			break;
		}
		case EFacing::Back:
		{
			vertices[0] = FVector(-range.X, -range.Y, -range.Z) * 0.5f;
			vertices[1] = FVector(-range.X, -range.Y, range.Z) * 0.5f;
			vertices[2] = FVector(-range.X, range.Y, range.Z) * 0.5f;
			vertices[3] = FVector(-range.X, range.Y, -range.Z) * 0.5f;
			break;
		}
		case EFacing::Left:
		{
			vertices[0] = FVector(range.X, -range.Y, -range.Z) * 0.5f;
			vertices[1] = FVector(range.X, -range.Y, range.Z) * 0.5f;
			vertices[2] = FVector(-range.X, -range.Y, range.Z) * 0.5f;
			vertices[3] = FVector(-range.X, -range.Y, -range.Z) * 0.5f;
			break;
		}
		case EFacing::Right:
		{
			vertices[0] = FVector(-range.X, range.Y, -range.Z) * 0.5f;
			vertices[1] = FVector(-range.X, range.Y, range.Z) * 0.5f;
			vertices[2] = FVector(range.X, range.Y, range.Z) * 0.5f;
			vertices[3] = FVector(range.X, range.Y, -range.Z) * 0.5f;
			break;
		}
	}

	BuildFace(InVoxelItem, vertices, (int32)InFacing, UVoxelModuleBPLibrary::DirectionToVector((EDirection)InFacing));
}

void UVoxelMeshComponent::BuildFace(const FVoxelItem& InVoxelItem, FVector InVertices[4], int InFaceIndex, FVector InNormal)
{
	int32 verNum = Vertices.Num();
	UVoxelAssetBase voxelData = InVoxelItem.GetData<UVoxelAssetBase>();
	FVector2D uvCorner = voxelData.GetUVCorner(InFaceIndex, AVoxelModule::GetWorldData().GetChunkMaterial(voxelData.Transparency).BlockUVSize);
	FVector2D uvSpan = voxelData.GetUVSpan(InFaceIndex, AVoxelModule::GetWorldData().GetChunkMaterial(voxelData.Transparency).BlockUVSize);
	InNormal = InVoxelItem.Rotation.RotateVector(InNormal);

	FVector center = voxelData.GetCeilRange(InVoxelItem.Rotation, InVoxelItem.Scale) * (CenterOffset + InVoxelItem.Rotation.RotateVector(voxelData.Offset) * OffsetScale);
	for (int32 i = 0; i < 4; i++)
	{
		Vertices.Add((InVoxelItem.Index.ToVector() + center + InVoxelItem.Rotation.RotateVector(InVertices[i] * InVoxelItem.Scale)) * AVoxelModule::GetWorldData().BlockSize * BlockScale);
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

bool UVoxelMeshComponent::IsEmpty() const
{
	return Vertices.Num() == 0;
}

AVoxelChunk* UVoxelMeshComponent::GetOwnerChunk() const
{
	return Cast<AVoxelChunk>(GetOwner());
}
