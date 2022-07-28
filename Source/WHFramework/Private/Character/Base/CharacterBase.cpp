// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/Base/CharacterBase.h"

#include "AIController.h"
#include "Main/MainModule.h"
#include "Animation/AnimInstance.h"
#include "Components/AudioComponent.h"
#include "Asset/AssetModuleBPLibrary.h"
#include "Audio/AudioModuleBPLibrary.h"
#include "Character/CharacterModuleNetworkComponent.h"
#include "Character/Base/CharacterAnim.h"
#include "Character/Base/CharacterDataBase.h"
#include "Scene/SceneModuleBPLibrary.h"
#include "Net/UnrealNetwork.h"
#include "Tasks/AITask_MoveTo.h"
#include "Voxel/VoxelModule.h"
#include "Voxel/VoxelModuleBPLibrary.h"
#include "Voxel/Chunks/VoxelChunk.h"
#include "Voxel/Datas/VoxelData.h"

ACharacterBase::ACharacterBase()
{
	bReplicates = true;

	Name = NAME_None;
	Anim = nullptr;
	DefaultController = nullptr;

	ActorID = FGuid::NewGuid();
	bVisible = true;
	Container = nullptr;
}

void ACharacterBase::BeginPlay()
{
	Super::BeginPlay();

	Anim = Cast<UCharacterAnim>(GetMesh()->GetAnimInstance());
}

void ACharacterBase::OnSpawn_Implementation(const TArray<FParameter>& InParams)
{
	Execute_SetActorVisible(this, true);

	if(Execute_IsAddToList(this))
	{
		USceneModuleBPLibrary::AddSceneActor(this);
	}
}

void ACharacterBase::OnDespawn_Implementation()
{
	Execute_SetActorVisible(this, false);

	SetActorLocationAndRotation(FVector::ZeroVector, FRotator::ZeroRotator);

	if(Execute_IsAddToList(this))
	{
		USceneModuleBPLibrary::RemoveSceneActor(this);
	}
	else if(Container)
	{
		Container->RemoveSceneActor(this);
	}

	Container = nullptr;
	DefaultController = nullptr;
}

void ACharacterBase::SpawnDefaultController()
{
	Super::SpawnDefaultController();

	DefaultController = Controller;
}

void ACharacterBase::SetActorVisible_Implementation(bool bNewVisible)
{
	bVisible = bNewVisible;
	GetRootComponent()->SetVisibility(bNewVisible, true);
	TArray<AActor*> AttachedActors;
	GetAttachedActors(AttachedActors);
	for(auto Iter : AttachedActors)
	{
		ISceneActorInterface::Execute_SetActorVisible(Iter, bNewVisible);
	}
}

bool ACharacterBase::GenerateVoxel(FVoxelItem& InVoxelItem)
{
	bool bSuccess = false;
	AVoxelChunk* chunk = InVoxelItem.Owner;
	const FIndex index = InVoxelItem.Index;
	const FVoxelItem& voxelItem = chunk->GetVoxelItem(index);

	if(!voxelItem.IsValid() || voxelItem.GetData<UVoxelData>().Transparency == EVoxelTransparency::Transparent && voxelItem != InVoxelItem)
	{
		FHitResult HitResult;
		if (!AMainModule::GetModuleByClass<AVoxelModule>()->VoxelTraceSingle(InVoxelItem, chunk->IndexToLocation(index), HitResult))
		{
			if(voxelItem.IsValid())
			{
				bSuccess = chunk->ReplaceVoxel(voxelItem, InVoxelItem);
			}
			else
			{
				bSuccess = chunk->GenerateVoxel(index, InVoxelItem);
			}
		}
	}
	return bSuccess;
}

bool ACharacterBase::GenerateVoxel(FVoxelItem& InVoxelItem, const FVoxelHitResult& InVoxelHitResult)
{
	bool bSuccess = false;
	AVoxelChunk* chunk = InVoxelHitResult.GetOwner();
	const FIndex index = chunk->LocationToIndex(InVoxelHitResult.Point - UVoxelModuleBPLibrary::GetWorldData().GetBlockSizedNormal(InVoxelHitResult.Normal)) + FIndex(InVoxelHitResult.Normal);
	const FVoxelItem& voxelItem = chunk->GetVoxelItem(index);

	if(!voxelItem.IsValid() || voxelItem.GetData<UVoxelData>().Transparency == EVoxelTransparency::Transparent && voxelItem != InVoxelHitResult.VoxelItem)
	{
		FHitResult HitResult;
		if (!AMainModule::GetModuleByClass<AVoxelModule>()->VoxelTraceSingle(InVoxelItem, chunk->IndexToLocation(index), HitResult))
		{
			if(voxelItem.IsValid())
			{
				bSuccess = chunk->ReplaceVoxel(voxelItem, InVoxelItem);
			}
			else
			{
				bSuccess = chunk->GenerateVoxel(index, InVoxelItem);
			}
		}
	}
	return bSuccess;
}

bool ACharacterBase::DestroyVoxel(FVoxelItem& InVoxelItem)
{
	if (InVoxelItem.GetData<UVoxelData>().VoxelType != EVoxelType::Bedrock)
	{
		return InVoxelItem.Owner->DestroyVoxel(InVoxelItem);
	}
	return false;
}

bool ACharacterBase::DestroyVoxel(const FVoxelHitResult& InVoxelHitResult)
{
	AVoxelChunk* Chunk = InVoxelHitResult.GetOwner();
	const FVoxelItem& VoxelItem = InVoxelHitResult.VoxelItem;

	if (VoxelItem.GetData<UVoxelData>().VoxelType != EVoxelType::Bedrock)
	{
		return Chunk->DestroyVoxel(VoxelItem);
	}
	return false;
}

void ACharacterBase::PlaySound(USoundBase* InSound, float InVolume, bool bMulticast)
{
	UAudioModuleBPLibrary::PlaySingleSoundAtLocation(InSound, Name, GetActorLocation(), InVolume, bMulticast);
}

void ACharacterBase::StopSound(bool bMulticast)
{
	UAudioModuleBPLibrary::StopSingleSound(Name, bMulticast);
}

void ACharacterBase::PlayMontage(UAnimMontage* InMontage, bool bMulticast)
{
	if(bMulticast)
	{
		if(HasAuthority())
		{
			MultiPlayMontage(InMontage);
		}
		else if(UCharacterModuleNetworkComponent* CharacterModuleNetworkComponent = AMainModule::GetModuleNetworkComponentByClass<UCharacterModuleNetworkComponent>())
		{
			CharacterModuleNetworkComponent->ServerPlayMontageMulticast(this, InMontage);
		}
		return;
	}
	if(Anim)
	{
		Anim->Montage_Play(InMontage);
	}
}

void ACharacterBase::MultiPlayMontage_Implementation(UAnimMontage* InMontage)
{
	PlayMontage(InMontage, false);
}

void ACharacterBase::PlayMontageByName(const FName InMontageName, bool bMulticast)
{
	UCharacterDataBase& CharacterData = UAssetModuleBPLibrary::LoadPrimaryAssetRef<UCharacterDataBase>(AssetID);
	if(CharacterData.AnimMontages.Contains(InMontageName))
	{
		PlayMontage(CharacterData.AnimMontages[InMontageName], bMulticast);
	}
}

void ACharacterBase::StopMontage(UAnimMontage* InMontage, bool bMulticast)
{
	if(bMulticast)
	{
		if(HasAuthority())
		{
			MultiStopMontage(InMontage);
		}
		else if(UCharacterModuleNetworkComponent* CharacterModuleNetworkComponent = AMainModule::GetModuleNetworkComponentByClass<UCharacterModuleNetworkComponent>())
		{
			CharacterModuleNetworkComponent->ServerStopMontageMulticast(this, InMontage);
		}
		return;
	}
	if(Anim)
	{
		Anim->Montage_Stop(.2f, InMontage);
	}
}

void ACharacterBase::MultiStopMontage_Implementation(UAnimMontage* InMontage)
{
	StopMontage(InMontage, false);
}

void ACharacterBase::StopMontageByName(const FName InMontageName, bool bMulticast)
{
	UCharacterDataBase& CharacterData = UAssetModuleBPLibrary::LoadPrimaryAssetRef<UCharacterDataBase>(AssetID);
	if(CharacterData.AnimMontages.Contains(InMontageName))
	{
		StopMontage(CharacterData.AnimMontages[InMontageName], bMulticast);
	}
}

void ACharacterBase::TransformTowards(FTransform InTransform, float InDuration, bool bMulticast)
{
	if(bMulticast)
	{
		if(HasAuthority())
		{
			MultiTransformTowards(InTransform);
		}
		else if(UCharacterModuleNetworkComponent* CharacterModuleNetworkComponent = AMainModule::GetModuleNetworkComponentByClass<UCharacterModuleNetworkComponent>())
		{
			CharacterModuleNetworkComponent->ServerTransformTowardsToMulticast(this, InTransform);
		}
		return;
	}
	SetActorTransform(InTransform);
}

void ACharacterBase::MultiTransformTowards_Implementation(FTransform InTransform, float InDuration)
{
	TransformTowards(InTransform, false);
}

void ACharacterBase::RotationTowards(FRotator InRotation, float InDuration, bool bMulticast)
{
	if(bMulticast)
	{
		if(HasAuthority())
		{
			MultiRotationTowards(InRotation, InDuration);
		}
		else if(UCharacterModuleNetworkComponent* CharacterModuleNetworkComponent = AMainModule::GetModuleNetworkComponentByClass<UCharacterModuleNetworkComponent>())
		{
			CharacterModuleNetworkComponent->ServerRotationTowardsMulticast(this, InRotation, InDuration);
		}
		return;
	}
	SetActorRotation(InRotation);
}

void ACharacterBase::MultiRotationTowards_Implementation(FRotator InRotation, float InDuration)
{
	RotationTowards(InRotation, InDuration, false);
}

void ACharacterBase::AIMoveTo(FVector InLocation, float InStopDistance, bool bMulticast)
{
	if(bMulticast)
	{
		if(HasAuthority())
		{
			MultiAIMoveTo(InLocation, InStopDistance);
		}
		else if(UCharacterModuleNetworkComponent* CharacterModuleNetworkComponent = AMainModule::GetModuleNetworkComponentByClass<UCharacterModuleNetworkComponent>())
		{
			CharacterModuleNetworkComponent->ServerAIMoveToMulticast(this, InLocation, InStopDistance);
		}
		return;
	}
	if(AAIController* AIController = GetController<AAIController>())
	{
		AIController->MoveToLocation(InLocation, InStopDistance);
	}
}

void ACharacterBase::MultiAIMoveTo_Implementation(FVector InLocation, float InStopDistance)
{
	AIMoveTo(InLocation, InStopDistance, false);
}

void ACharacterBase::StopAIMove(bool bMulticast)
{
	if(bMulticast)
	{
		if(HasAuthority())
		{
			MultiStopAIMove();
		}
		else if(UCharacterModuleNetworkComponent* CharacterModuleNetworkComponent = AMainModule::GetModuleNetworkComponentByClass<UCharacterModuleNetworkComponent>())
		{
			CharacterModuleNetworkComponent->ServerStopAIMoveMulticast(this);
		}
		return;
	}
	if(AAIController* AIController = GetController<AAIController>())
	{
		AIController->StopMovement();
	}
}

void ACharacterBase::MultiStopAIMove_Implementation()
{
	StopAIMove(false);
}

UBehaviorTree* ACharacterBase::GetBehaviorTreeAsset() const
{
	return nullptr;
}

void ACharacterBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
}
