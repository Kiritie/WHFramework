// Fill out your copyright notice in the Description page of Project Settings.


#include "Voxel/Voxels/Voxel.h"

#include "Ability/AbilityModuleBPLibrary.h"
#include "Asset/AssetModuleBPLibrary.h"
#include "Asset/AssetModuleTypes.h"
#include "Asset/AssetManagerBase.h"
#include "Audio/AudioModuleBPLibrary.h"
#include "Global/GlobalBPLibrary.h"
#include "Voxel/Datas/VoxelData.h"
#include "Kismet/GameplayStatics.h"
#include "Math/MathBPLibrary.h"
#include "ObjectPool/ObjectPoolModuleBPLibrary.h"
#include "Voxel/VoxelModuleBPLibrary.h"
#include "Voxel/Agent/VoxelAgentInterface.h"
#include "Voxel/Chunks/VoxelChunk.h"
#include "Widget/WidgetModuleBPLibrary.h"

UVoxel::UVoxel()
{
	ID = FPrimaryAssetId();
	Index = FIndex::ZeroIndex;
	Angle = ERightAngle::RA_0;
	Owner = nullptr;
	Auxiliary = nullptr;
	bGenerated = false;
}

UVoxel& UVoxel::GetEmpty()
{
	return UReferencePoolModuleBPLibrary::GetReference<UVoxelEmpty>();
}

UVoxel& UVoxel::GetUnknown()
{
	return UReferencePoolModuleBPLibrary::GetReference<UVoxelUnknown>();
}

void UVoxel::OnReset_Implementation()
{
	ID = FPrimaryAssetId();
	Index = FIndex::ZeroIndex;
	Angle = ERightAngle::RA_0;
	Owner = nullptr;
	Auxiliary = nullptr;
	bGenerated = false;
}

void UVoxel::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
}

void UVoxel::LoadData(FSaveData* InSaveData, bool bForceMode)
{
	const auto& SaveData = InSaveData->CastRef<FVoxelItem>();
	ID = SaveData.ID;
	Index = SaveData.Index;
	Angle = SaveData.Angle;
	Owner = SaveData.Owner;
	Auxiliary = SaveData.Auxiliary;
	bGenerated = SaveData.bGenerated;
}

FSaveData* UVoxel::ToData()
{
	static FVoxelItem SaveData;
	SaveData = FVoxelItem();

	SaveData.ID = ID;
	SaveData.Index = Index;
	SaveData.Angle = Angle;
	SaveData.Owner = Owner;
	SaveData.Auxiliary = Auxiliary;
	SaveData.bGenerated = bGenerated;

	return &SaveData;
}

void UVoxel::RefreshData()
{
	GetItem().RefreshData(this);
}

void UVoxel::OnGenerate(IVoxelAgentInterface* InAgent)
{
	if(InAgent)
	{
		if(GetData().PartType == EVoxelPartType::Main)
		{
			UAudioModuleBPLibrary::PlaySoundAtLocation(GetData().GenerateSound, GetLocation());
		}
	}
}

void UVoxel::OnDestroy(IVoxelAgentInterface* InAgent)
{
	if(InAgent)
	{
		if(GetData().PartType == EVoxelPartType::Main)
		{
			UAudioModuleBPLibrary::PlaySoundAtLocation(GetData().DestroySound, GetLocation());
			UAbilityModuleBPLibrary::SpawnPickUp(FAbilityItem(ID, 1), GetLocation() + GetData().GetRange(Angle) * UVoxelModuleBPLibrary::GetWorldData().BlockSize * 0.5f, Owner);
		}
		if(!Owner->CheckVoxelAdjacent(Index, EDirection::Up))
		{
			Owner->SetVoxelComplex(UMathBPLibrary::GetAdjacentIndex(Index, EDirection::Up, Angle), FVoxelItem::Empty, true, InAgent);
		}
	}
}

void UVoxel::OnAgentHit(IVoxelAgentInterface* InAgent, const FVoxelHitResult& InHitResult)
{
	
}

void UVoxel::OnAgentEnter(IVoxelAgentInterface* InAgent, const FVoxelHitResult& InHitResult)
{
	
}

void UVoxel::OnAgentStay(IVoxelAgentInterface* InAgent, const FVoxelHitResult& InHitResult)
{
	
}

void UVoxel::OnAgentExit(IVoxelAgentInterface* InAgent, const FVoxelHitResult& InHitResult)
{
	
}

bool UVoxel::OnActionTrigger(IVoxelAgentInterface* InAgent, EVoxelActionType InActionType, const FVoxelHitResult& InHitResult)
{
	switch (InActionType)
	{
		case EVoxelActionType::Action1:
		{
			return InAgent->DestroyVoxel(InHitResult);
		}
		case EVoxelActionType::Action2:
		{
			return InAgent->GenerateVoxel(InHitResult);
		}
		default: break;
	}
	return false;
}

bool UVoxel::IsValid() const
{
	return !IsEmpty() && !IsUnknown();
}

bool UVoxel::IsEmpty() const
{
	return &UVoxel::GetEmpty() == this;
}

bool UVoxel::IsUnknown() const
{
	return &UVoxel::GetUnknown() == this;
}

FVector UVoxel::GetLocation(bool bWorldSpace) const
{
	if(Owner)
	{
		return Owner->IndexToLocation(Index, bWorldSpace);
	}
	return FVector::ZeroVector;
}

UVoxelData& UVoxel::GetData() const
{
	return UAssetModuleBPLibrary::LoadPrimaryAssetRef<UVoxelData>(ID);
}

FVoxelItem& UVoxel::GetItem()
{
	if(Owner) return Owner->GetVoxelItem(Index);
	return ToSaveDataRef<FVoxelItem>(true);
}
