// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Components/BoxComponent.h"
#include "Voxel/VoxelModuleTypes.h"

#include "CharacterPartBase.generated.h"

class ACharacterBase;
class UVoxel;
/**
 * 角色部位组件
 */
UCLASS(ClassGroup="Collision", editinlinenew, meta=(DisplayName="Character Part", BlueprintSpawnableComponent))
class WHFRAMEWORK_API UCharacterPartBase : public UBoxComponent
{
	GENERATED_BODY()
	
public:
	UCharacterPartBase(const FObjectInitializer& ObjectInitializer);
	
protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	virtual void UpdateVoxelOverlap();

protected:
	virtual void OnHitVoxel(UVoxel& InVoxel, const FVoxelHitResult& InHitResult);

	virtual void OnEnterVoxel(UVoxel& InVoxel, const FVoxelHitResult& InHitResult);

	virtual void OnStayVoxel(UVoxel& InVoxel, const FVoxelHitResult& InHitResult);

	virtual void OnExitVoxel(UVoxel& InVoxel, const FVoxelHitResult& InHitResult);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FVoxelItem OverlappingVoxel;

public:
	UFUNCTION(BlueprintPure)
	FVoxelItem& GetOverlappingVoxel() { return OverlappingVoxel; }
	
	template<class T>
	T* GetOwnerCharacter() const
	{
		return Cast<T>(GetOwnerCharacter());
	}

	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"))
	virtual ACharacterBase* GetOwnerCharacter(TSubclassOf<ACharacterBase> InClass = nullptr) const;
};
