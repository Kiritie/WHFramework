// Fill out your copyright notice in the Description page of Project Settings.


#include "Voxel/Voxels/Voxel.h"

#include "Asset/AssetModuleBPLibrary.h"
#include "Asset/AssetModuleTypes.h"
#include "Asset/AssetManagerBase.h"
#include "Global/GlobalBPLibrary.h"
#include "Voxel/Datas/VoxelData.h"
#include "Kismet/GameplayStatics.h"
#include "ObjectPool/ObjectPoolModuleBPLibrary.h"
#include "Voxel/VoxelModuleBPLibrary.h"
#include "Voxel/Agent/VoxelAgentInterface.h"
#include "Voxel/Chunks/VoxelChunk.h"
#include "Widget/WidgetModuleBPLibrary.h"

UVoxel::UVoxel()
{
	Index = FIndex::ZeroIndex;
	Rotation = FRotator::ZeroRotator;
	Scale = FVector::OneVector;
	Owner = nullptr;
	Auxiliary = nullptr;
}

UVoxel& UVoxel::GetEmpty()
{
	return UReferencePoolModuleBPLibrary::GetReference<UVoxelEmpty>();
}

UVoxel& UVoxel::GetUnknown()
{
	return UReferencePoolModuleBPLibrary::GetReference<UVoxelUnknown>();
}

void UVoxel::OnSpawn_Implementation(const TArray<FParameter>& InParams)
{
	
}

void UVoxel::OnDespawn_Implementation()
{
	ID = FPrimaryAssetId();
	Index = FIndex::ZeroIndex;
	Rotation = FRotator::ZeroRotator;
	Scale = FVector::OneVector;
	Owner = nullptr;
	Auxiliary = nullptr;
}

void UVoxel::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if(Ar.ArIsSaveGame)
	{
		if(Ar.IsLoading())
		{
			Ar << Index;
			Ar << Rotation;
			Ar << Scale;
		}
		else if(Ar.IsSaving())
		{
			Ar << Index;
			Ar << Rotation;
			Ar << Scale;
		}
	}
}

void UVoxel::LoadData(FSaveData* InSaveData, bool bForceMode)
{
	const auto SaveData = InSaveData->CastRef<FVoxelItem>();

	ID = SaveData.ID;
	Index = SaveData.Index;
	Rotation = SaveData.Rotation;
	Scale = SaveData.Scale;
}

FSaveData* UVoxel::ToData()
{
	static FVoxelItem SaveData;
	SaveData.Reset();

	SaveData.VoxelType = UVoxelModuleBPLibrary::GetVoxelTypeByAssetID(ID);
	SaveData.ID = ID;
	SaveData.Index = Index;
	SaveData.Rotation = Rotation;
	SaveData.Scale = Scale;
	SaveData.Owner = Owner;
	SaveData.Auxiliary = Auxiliary;

	return &SaveData;
}

void UVoxel::RefreshData()
{
	if(Owner)
	{
		Owner->GetVoxelItem(Index).RefreshSaveData();
	}
}

void UVoxel::OnTargetHit(IVoxelAgentInterface* InTarget, const FVoxelHitResult& InHitResult)
{
	
}

void UVoxel::OnTargetEnter(IVoxelAgentInterface* InTarget, const FVoxelHitResult& InHitResult)
{
	
}

void UVoxel::OnTargetStay(IVoxelAgentInterface* InTarget, const FVoxelHitResult& InHitResult)
{
	
}

void UVoxel::OnTargetExit(IVoxelAgentInterface* InTarget, const FVoxelHitResult& InHitResult)
{
	
}

bool UVoxel::OnMouseDown(EMouseButton InMouseButton, const FVoxelHitResult& InHitResult)
{
	if(!Owner || !Owner->IsValidLowLevel()) return false;

	switch (InMouseButton)
	{
		case EMouseButton::Left:
		{
			if(IVoxelAgentInterface* VoxelAgentPlayer = UGlobalBPLibrary::GetPlayerCharacter<IVoxelAgentInterface>())
			{
				return VoxelAgentPlayer->DestroyVoxel(InHitResult);
			}
			break;
		}
		case EMouseButton::Right:
		{
			if(IVoxelAgentInterface* VoxelAgentPlayer = UGlobalBPLibrary::GetPlayerCharacter<IVoxelAgentInterface>())
			{
				return VoxelAgentPlayer->GenerateVoxel(VoxelAgentPlayer->GetGenerateVoxelItem(), InHitResult);
			}
			break;
		}
		default: break;
	}
	return false;
}

bool UVoxel::OnMouseUp(EMouseButton InMouseButton, const FVoxelHitResult& InHitResult)
{
	return false;
}

bool UVoxel::OnMouseHold(EMouseButton InMouseButton, const FVoxelHitResult& InHitResult)
{
	return false;
}

void UVoxel::OnMouseHover(const FVoxelHitResult& InHitResult)
{
	
}

bool UVoxel::IsEmpty() const
{
	return &UVoxel::GetEmpty() == this;
}

bool UVoxel::IsUnknown() const
{
	return &UVoxel::GetUnknown() == this;
}

UVoxelData& UVoxel::GetData() const
{
	return UAssetModuleBPLibrary::LoadPrimaryAssetRef<UVoxelData>(ID);
}
