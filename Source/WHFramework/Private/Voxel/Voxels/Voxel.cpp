// Fill out your copyright notice in the Description page of Project Settings.


#include "Voxel/Voxels/Voxel.h"

#include "Asset/AssetModuleBPLibrary.h"
#include "Asset/AssetModuleTypes.h"
#include "Asset/Primary/PrimaryAssetManager.h"
#include "Voxel/Voxels/VoxelAssetBase.h"
#include "Kismet/GameplayStatics.h"
#include "ObjectPool/ObjectPoolModuleBPLibrary.h"
#include "Voxel/Agent/VoxelAgentInterface.h"
#include "Voxel/Chunks/VoxelChunk.h"
#include "Widget/WidgetModuleBPLibrary.h"

UVoxel* UVoxel::EmptyVoxel = nullptr;

UVoxel* UVoxel::UnknownVoxel = nullptr;

UVoxel::UVoxel()
{
	Index = FIndex::ZeroIndex;
	Rotation = FRotator::ZeroRotator;
	Scale = FVector::OneVector;
	Params = TMap<FName, FParameter>();
	Owner = nullptr;
	Auxiliary = nullptr;
}

UVoxel* UVoxel::SpawnVoxel(EVoxelType InVoxelType)
{
	return SpawnVoxel(FPrimaryAssetId::FromString(FString::Printf(TEXT("Voxel:DA_Voxel_%s"), *UGlobalBPLibrary::GetEnumValueAuthoredName(TEXT("EVoxelType"), (int32)InVoxelType))));
	return nullptr;
}

UVoxel* UVoxel::SpawnVoxel(const FPrimaryAssetId& InVoxelID)
{
	UVoxel* voxel = nullptr;
	UVoxelAssetBase* voxelData = UAssetModuleBPLibrary::LoadPrimaryAsset<UVoxelAssetBase>(InVoxelID);
	if(voxelData)
	{
		const TSubclassOf<UVoxel> tmpClass = voxelData->VoxelClass ? voxelData->VoxelClass : StaticClass();
		voxel = UObjectPoolModuleBPLibrary::SpawnObject<UVoxel>(tmpClass);
		if(voxel) voxel->ID = InVoxelID;
	}
	return voxel;
}

UVoxel* UVoxel::LoadVoxel(AVoxelChunk* InOwner, const FVoxelItem& InVoxelItem)
{
	UVoxel* voxel = SpawnVoxel(InVoxelItem.ID);
	if(voxel)
	{
		voxel->LoadItem(InVoxelItem);
		voxel->SetOwner(InOwner);
		voxel->SetAuxiliary(InVoxelItem.Auxiliary);
	}
	return voxel;
}

UVoxel* UVoxel::LoadVoxel(AVoxelChunk* InOwner, const FString& InVoxelData)
{
	FString str1, str2;
	InVoxelData.Split(TEXT(";"), &str1, &str2);
	UVoxel* voxel = SpawnVoxel(FPrimaryAssetId::FromString(str1));
	if(voxel)
	{
		voxel->LoadData(str2);
		voxel->SetOwner(InOwner);
	}
	return voxel;
}

void UVoxel::DespawnVoxel(UVoxel* InVoxel)
{
	if(IsValid(InVoxel)) UObjectPoolModuleBPLibrary::DespawnObject(InVoxel);
}

bool UVoxel::IsValid(UVoxel* InVoxel)
{
	return InVoxel && InVoxel->IsValidLowLevel() && InVoxel != EmptyVoxel && InVoxel != UnknownVoxel;
}

void UVoxel::LoadData(const FString& InValue)
{
	TArray<FString> data;
	InValue.ParseIntoArray(data, TEXT(";"));

	Index = FIndex(data[0]);
	
	TArray<FString> rotData;
	data[1].ParseIntoArray(rotData, TEXT(","));
	Rotation = FRotator(FCString::Atof(*rotData[0]), FCString::Atof(*rotData[1]), FCString::Atof(*rotData[2]));
	
	TArray<FString> scaleData;
	data[2].ParseIntoArray(scaleData, TEXT(","));
	Scale = FVector(FCString::Atof(*scaleData[0]), FCString::Atof(*scaleData[1]), FCString::Atof(*scaleData[2]));
}

void UVoxel::LoadItem(const FVoxelItem& InVoxelItem)
{
	ID = InVoxelItem.ID;
	Index = InVoxelItem.Index;
	Rotation = InVoxelItem.Rotation;
	Scale = InVoxelItem.Scale;
	Params = InVoxelItem.Params;
	Owner = InVoxelItem.Owner;
	Auxiliary = InVoxelItem.Auxiliary;
}

FString UVoxel::ToData()
{
	return FString::Printf(TEXT("%s;%s;%s;%s"), *ID.ToString(), *Index.ToString(), *FString::Printf(TEXT("%f,%f,%f"), Rotation.Pitch, Rotation.Yaw, Rotation.Roll), *FString::Printf(TEXT("%f,%f,%f"), Scale.X, Scale.Y, Scale.Z));
}

FVoxelItem UVoxel::ToItem()
{
	FVoxelItem voxelItem;
	voxelItem.ID = ID;
	voxelItem.Index = Index;
	voxelItem.Rotation = Rotation;
	voxelItem.Scale = Scale;
	voxelItem.Owner = Owner;
	voxelItem.Auxiliary = Auxiliary;
	voxelItem.Params = Params;
	return voxelItem;
}

void UVoxel::OnSpawn_Implementation()
{
	
}

void UVoxel::OnDespawn_Implementation()
{
	ID = FPrimaryAssetId();
	Index = FIndex::ZeroIndex;
	Rotation = FRotator::ZeroRotator;
	Scale = FVector::OneVector;
	Params.Empty();
	Owner = nullptr;
	Auxiliary = nullptr;
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
			if(IVoxelAgentInterface* VoxelAgentPlayer = UGlobalBPLibrary::GetPlayerCharacter<IVoxelAgentInterface>(this))
			{
				return VoxelAgentPlayer->DestroyVoxel(InHitResult);
			}
			break;
		}
		case EMouseButton::Right:
		{
			if(IVoxelAgentInterface* VoxelAgentPlayer = UGlobalBPLibrary::GetPlayerCharacter<IVoxelAgentInterface>(this))
			{
				FItem tmpItem = FItem(VoxelAgentPlayer->GetGeneratingVoxelItem(), 1);
				return VoxelAgentPlayer->GenerateVoxel(InHitResult, tmpItem);
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

UVoxelAssetBase* UVoxel::GetData() const
{
	return UAssetModuleBPLibrary::LoadPrimaryAsset<UVoxelAssetBase>(ID);
}
