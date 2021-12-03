
#include "Scene/SceneModule.h"

#include "Debug/DebugModuleTypes.h"
#include "Engine/TargetPoint.h"
#include "Scene/Object/SceneObject.h"
#include "Scene/Object/PhysicsVolume/PhysicsVolumeBase.h"

ASceneModule::ASceneModule()
{
	ModuleName = FName("SceneModule");

	SceneObjects = TMap<FName, TScriptInterface<ISceneObject>>();

	TargetPoints = TMap<FName, ATargetPoint*>();
	
	ScenePoints = TMap<FName, USceneComponent*>();

	PhysicsVolumes = TMap<FName, APhysicsVolumeBase*>();
	
	DefaultPhysicsVolumes = TArray<FPhysicsVolumeData>();

	FPhysicsVolumeData WaterPhysicsVolume;
	WaterPhysicsVolume.Name = FName("Water");
	WaterPhysicsVolume.Priority = 1.f;
	WaterPhysicsVolume.bWaterVolume = true;
	WaterPhysicsVolume.PhysicsVolumeClass = APhysicsVolumeBase::StaticClass();
	DefaultPhysicsVolumes.Add(WaterPhysicsVolume);
}

#if WITH_EDITOR
void ASceneModule::OnGenerate_Implementation()
{
	Super::OnGenerate_Implementation();
}

void ASceneModule::OnDestroy_Implementation()
{
	Super::OnDestroy_Implementation();
}
#endif

void ASceneModule::OnInitialize_Implementation()
{
	Super::OnInitialize_Implementation();

	for(auto Iter : DefaultPhysicsVolumes)
	{
		FActorSpawnParameters ActorSpawnParameters;
		ActorSpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if(APhysicsVolumeBase* PhysicsVolume = GetWorld()->SpawnActor<APhysicsVolumeBase>(ActorSpawnParameters))
		{
			PhysicsVolume->Initialize(Iter);
			PhysicsVolumes.Add(Iter.Name, PhysicsVolume);
		}
	}
}

void ASceneModule::OnRefresh_Implementation(float DeltaSeconds)
{
	Super::OnRefresh_Implementation(DeltaSeconds);
}

void ASceneModule::OnPause_Implementation()
{
	Super::OnPause_Implementation();
}

void ASceneModule::OnUnPause_Implementation()
{
	Super::OnUnPause_Implementation();
}

bool ASceneModule::HasSceneObject(FName InName, bool bEnsured) const
{
	if(SceneObjects.Contains(InName)) return true;
	return bEnsured ? ensureEditor(false) : false;
}

TScriptInterface<ISceneObject> ASceneModule::GetSceneObject(FName InName, bool bEnsured) const
{
	if(HasSceneObject(InName, bEnsured))
	{
		return SceneObjects[InName];
	}
	return nullptr;
}

void ASceneModule::SetSceneObject(FName InName, TScriptInterface<ISceneObject> InObject)
{
	if(SceneObjects.Contains(InName))
	{
		SceneObjects[InName] = InObject;
	}
	else
	{
		SceneObjects.Add(InName, InObject);
	}
	if(InObject.GetObject() && InObject.GetObject()->IsValidLowLevel())
	{
		SetScenePoint(InName, InObject->Execute_GetScenePoint(InObject.GetObject()));
	}
}

bool ASceneModule::HasTargetPoint(FName InName, bool bEnsured) const
{
	if(TargetPoints.Contains(InName)) return true;
	return bEnsured ? ensureEditor(false) : false;
}

ATargetPoint* ASceneModule::GetTargetPoint(FName InName, bool bEnsured) const
{
	if(HasTargetPoint(InName, bEnsured))
	{
		return TargetPoints[InName];
	}
	return nullptr;
}

void ASceneModule::SetTargetPoint(FName InName, ATargetPoint* InPoint)
{
	if(TargetPoints.Contains(InName))
	{
		TargetPoints[InName] = InPoint;
	}
	else
	{
		TargetPoints.Add(InName, InPoint);
	}
	if(InPoint && InPoint->IsValidLowLevel())
	{
		SetScenePoint(InName, InPoint->GetRootComponent());
	}
}

bool ASceneModule::HasScenePoint(FName InName, bool bEnsured) const
{
	if(ScenePoints.Contains(InName)) return true;
	return bEnsured ? ensureEditor(false) : false;
}

USceneComponent* ASceneModule::GetScenePoint(FName InName, bool bEnsured) const
{
	if(HasScenePoint(InName, bEnsured))
	{
		return ScenePoints[InName];
	}
	return nullptr;
}

void ASceneModule::SetScenePoint(FName InName, USceneComponent* InSceneComp)
{
	if(ScenePoints.Contains(InName))
	{
		ScenePoints[InName] = InSceneComp;
	}
	else
	{
		ScenePoints.Add(InName, InSceneComp);
	}
}

bool ASceneModule::HasPhysicsVolume(FName InName, bool bEnsured) const
{
	if(PhysicsVolumes.Contains(InName)) return true;
	return bEnsured ? ensureEditor(false) : false;
}

APhysicsVolumeBase* ASceneModule::GetPhysicsVolume(FName InName, bool bEnsured) const
{
	if(HasPhysicsVolume(InName, bEnsured))
	{
		return PhysicsVolumes[InName];
	}
	return nullptr;
}

void ASceneModule::SetPhysicsVolume(FName InName, APhysicsVolumeBase* InPhysicsVolume)
{
	if(PhysicsVolumes.Contains(InName))
	{
		PhysicsVolumes[InName] = InPhysicsVolume;
	}
	else
	{
		PhysicsVolumes.Add(InName, InPhysicsVolume);
	}
}
