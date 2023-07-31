// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Input/InputModuleTypes.h"
#include "Voxel/VoxelModuleTypes.h"
#include "Global/Base/WHObject.h"
#include "SaveGame/Base/SaveDataInterface.h"

#include "Voxel.generated.h"

class IVoxelAgentInterface;
class AVoxelChunk;
class IVoxelAgentInterface;
class UVoxelData;

/**
 * ����
 */
UCLASS()
class WHFRAMEWORK_API UVoxel : public UWHObject, public ISaveDataInterface
{
private:
	GENERATED_BODY()

public:
	UVoxel();

	//////////////////////////////////////////////////////////////////////////
	// Statics
public:
	static UVoxel& GetEmpty();

	static UVoxel& GetUnknown();
	
	//////////////////////////////////////////////////////////////////////////
	// Defaults
public:
	virtual int32 GetLimit_Implementation() const override { return 10000; }

	virtual void OnReset_Implementation() override;

	virtual void Serialize(FArchive& Ar) override;

	virtual void LoadData(FSaveData* InSaveData, EPhase InPhase = EPhase::Final) override;

	virtual FSaveData* ToData(bool bRefresh) override;

	virtual void RefreshData();

	//////////////////////////////////////////////////////////////////////////
	// Events
public:
	virtual void OnGenerate(IVoxelAgentInterface* InAgent);

	virtual void OnDestroy(IVoxelAgentInterface* InAgent);

	virtual void OnAgentHit(IVoxelAgentInterface* InAgent, const FVoxelHitResult& InHitResult);

	virtual void OnAgentEnter(IVoxelAgentInterface* InAgent, const FVoxelHitResult& InHitResult);

	virtual void OnAgentStay(IVoxelAgentInterface* InAgent, const FVoxelHitResult& InHitResult);

	virtual void OnAgentExit(IVoxelAgentInterface* InAgent, const FVoxelHitResult& InHitResult);

	virtual bool OnActionTrigger(IVoxelAgentInterface* InAgent, EVoxelActionType InActionType, const FVoxelHitResult& InHitResult);

	//////////////////////////////////////////////////////////////////////////
	// Stats
protected:
	UPROPERTY(VisibleAnywhere)
	FPrimaryAssetId ID;

	UPROPERTY(VisibleAnywhere)
	FIndex Index;

	UPROPERTY(VisibleAnywhere)
	ERightAngle Angle;

	UPROPERTY(VisibleAnywhere)
	AVoxelChunk* Owner;

	UPROPERTY(VisibleAnywhere)
	AVoxelAuxiliary* Auxiliary;

	UPROPERTY(VisibleAnywhere)
	bool bGenerated;

public:
	bool IsValid() const;
	
	bool IsEmpty() const;

	bool IsUnknown() const;

	bool IsGenerated() const { return bGenerated; }

	FPrimaryAssetId GetID() const { return ID; }

	void SetID(FPrimaryAssetId InID) { ID = InID; }

	FIndex GetIndex() const { return Index; }

	FVector GetLocation(bool bWorldSpace = true) const;

	ERightAngle GetAngle() const { return Angle; }

	template<class T>
	T* GetOwner() const
	{
		return Cast<T>(Owner);
	}
	AVoxelChunk* GetOwner() const { return Owner; }

	template<class T>
	T* GetAuxiliary() const
	{
		return Cast<T>(Auxiliary);
	}
	AVoxelAuxiliary* GetAuxiliary() const { return Auxiliary; }

	template<class T>
	T& GetData() const
	{
		return static_cast<T&>(GetData());
	}
	UVoxelData& GetData() const;

	FVoxelItem& GetItem();
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
