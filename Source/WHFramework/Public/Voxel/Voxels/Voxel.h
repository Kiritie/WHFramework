// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Input/InputModuleTypes.h"
#include "Voxel/VoxelModuleTypes.h"
#include "Common/Base/WHObject.h"
#include "ReferencePool/ReferencePoolInterface.h"

#include "Voxel.generated.h"

class IVoxelAgentInterface;
class AVoxelChunk;
class UVoxelData;

/**
 * ����
 */
UCLASS()
class WHFRAMEWORK_API UVoxel : public UWHObject, public IReferencePoolInterface
{
private:
	GENERATED_BODY()

public:
	UVoxel();
	
	//////////////////////////////////////////////////////////////////////////
	// Reference
public:
	virtual void OnReset_Implementation() override;

	//////////////////////////////////////////////////////////////////////////
	// Voxel
public:
	static UVoxel& GetEmpty();

	static UVoxel& GetUnknown();

public:
	virtual void LoadData(const FString& InData);

	virtual FString ToData();

	virtual void RefreshData();

public:
	virtual void OnGenerate(IVoxelAgentInterface* InAgent);

	virtual void OnDestroy(IVoxelAgentInterface* InAgent);

	virtual void OnAgentHit(IVoxelAgentInterface* InAgent, const FVoxelHitResult& InHitResult);

	virtual void OnAgentEnter(IVoxelAgentInterface* InAgent, const FVoxelHitResult& InHitResult);

	virtual void OnAgentStay(IVoxelAgentInterface* InAgent, const FVoxelHitResult& InHitResult);

	virtual void OnAgentExit(IVoxelAgentInterface* InAgent, const FVoxelHitResult& InHitResult);

	virtual bool OnAgentInteract(IVoxelAgentInterface* InAgent, EInputInteractAction InInteractAction, const FVoxelHitResult& InHitResult);

protected:
	UPROPERTY(VisibleAnywhere)
	FVoxelItem Item;

public:
	bool IsValid() const;
	
	bool IsEmpty() const;

	bool IsUnknown() const;

	bool IsGenerated() const { return Item.bGenerated; }

	FVoxelItem& GetItem() { return Item; }

	void SetItem(const FVoxelItem& InItem) { Item = InItem; }

	FPrimaryAssetId GetID() const { return Item.ID; }

	FIndex GetIndex() const { return Item.Index; }

	ERightAngle GetAngle() const { return Item.Angle; }

	FVector GetLocation(bool bWorldSpace = true) const { return Item.GetLocation(bWorldSpace); }

	template<class T>
	T* GetOwner() const
	{
		return Cast<T>(Item.Owner);
	}
	AVoxelChunk* GetOwner() const { return Item.Owner; }

	template<class T>
	T* GetAuxiliary() const
	{
		return Cast<T>(Item.Auxiliary);
	}
	AVoxelAuxiliary* GetAuxiliary() const { return Item.Auxiliary; }

	template<class T>
	T& GetData() const
	{
		return static_cast<T&>(GetData());
	}
	UVoxelData& GetData() const { return Item.GetVoxelData(); }
};

UCLASS()
class WHFRAMEWORK_API UVoxelEmpty : public UVoxel
{
	GENERATED_BODY()
};

UCLASS()
class WHFRAMEWORK_API UVoxelUnknown : public UVoxel
{
	GENERATED_BODY()
};
