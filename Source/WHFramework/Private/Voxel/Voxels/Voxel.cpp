// Fill out your copyright notice in the Description page of Project Settings.


#include "Voxel/Voxels/Voxel.h"

#include "Asset/Primary/PrimaryAssetManager.h"
#include "Voxel/Voxels/VoxelAssetBase.h"
#include "Kismet/GameplayStatics.h"
#include "ObjectPool/ObjectPoolModuleBPLibrary.h"
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
	// return SpawnVoxel(*FString::Printf(TEXT("Voxel_%d"), (int32)InVoxelType));
	return nullptr;
}

UVoxel* UVoxel::SpawnVoxel(const FPrimaryAssetId& InVoxelID)
{
	const UVoxelAssetBase voxelData = UPrimaryAssetManager::LoadItemAsset<UVoxelAssetBase>(InVoxelID);
	const TSubclassOf<UVoxel> tmpClass = voxelData.VoxelClass ? voxelData.VoxelClass : StaticClass();
	auto voxel = UObjectPoolModuleBPLibrary::SpawnObject<UVoxel>(tmpClass);
	if(voxel) voxel->ID = InVoxelID;
	return voxel;
}

UVoxel* UVoxel::LoadVoxel(AVoxelChunk* InOwner, const FVoxelItem& InVoxelItem)
{
	auto voxel = SpawnVoxel(InVoxelItem.ID);
	voxel->LoadItem(InVoxelItem);
	voxel->SetOwner(InOwner);
	voxel->SetAuxiliary(InVoxelItem.Auxiliary);
	return voxel;
}

UVoxel* UVoxel::LoadVoxel(AVoxelChunk* InOwner, const FString& InVoxelData)
{
	FString str1, str2;
	InVoxelData.Split(TEXT(";"), &str1, &str2);
	auto voxel = SpawnVoxel(*str1);
	voxel->LoadData(str2);
	voxel->SetOwner(InOwner);
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

void UVoxel::OnTargetHit(ACharacterBase* InTarget, const FVoxelHitResult& InHitResult)
{
	
}

void UVoxel::OnTargetEnter(UAbilityCharacterPart* InTarget, const FVoxelHitResult& InHitResult)
{
	
}

void UVoxel::OnTargetStay(UAbilityCharacterPart* InTarget, const FVoxelHitResult& InHitResult)
{
	
}

void UVoxel::OnTargetExit(UAbilityCharacterPart* InTarget, const FVoxelHitResult& InHitResult)
{
	
}

bool UVoxel::OnMouseDown(EMouseButton InMouseButton, const FVoxelHitResult& InHitResult)
{
	if(!Owner || !Owner->IsValidLowLevel()) return false;

	switch (InMouseButton)
	{
		case EMouseButton::Left:
		{
			if(ADWPlayerCharacter* PlayerCharacter = UDWHelper::GetPlayerCharacter(this))
			{
				PlayerCharacter->DestroyVoxel(InHitResult);
			}
			break;
		}
		case EMouseButton::Right:
		{
			ADWPlayerCharacter* PlayerCharacter = UDWHelper::GetPlayerCharacter(this);
			auto tmpSlot = UWidgetModuleBPLibrary::GetUserWidget<UWidgetInventoryBar>()->GetSelectedSlot();
			if(PlayerCharacter && tmpSlot && !tmpSlot->IsEmpty())
			{
				FItem tmpItem = FItem(tmpSlot->GetItem(), 1);
				if(PlayerCharacter->GenerateVoxel(InHitResult, tmpItem))
				{
					tmpSlot->SubItem(tmpItem);
				}
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

UVoxelAssetBase UVoxel::GetVoxelData() const
{
	return UDWHelper::LoadVoxelData(ID);
}
