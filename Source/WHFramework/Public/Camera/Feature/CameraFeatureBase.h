#pragma once
#include "Camera/CameraModuleTypes.h"
#include "CameraFeatureBase.generated.h"

class ACameraManagerBase;

UCLASS(Abstract, Blueprintable, EditInlineNew)
class WHFRAMEWORK_API UCameraFeatureBase : public UObject
{
	GENERATED_BODY()

public:
	virtual void Enter(ACameraManagerBase* InManager, const FCameraFeatureContext& InContext);

	virtual void Exit();

	virtual void Apply(float DeltaTime, FCameraRigState& InOutState);

	int32 GetPriority() const { return Context.Priority; }

	const FCameraFeatureHandle& GetHandle() const { return Handle; }

	const FCameraFeatureContext& GetContext() const { return Context; }

	void SetHandle(const FCameraFeatureHandle& InHandle) { Handle = InHandle; }

	//////////////////////////////////////////////////////////////////////////
protected:
	UPROPERTY(Transient)
	TObjectPtr<ACameraManagerBase> CameraManager;

	UPROPERTY(Transient)
	FCameraFeatureContext Context;

	UPROPERTY(Transient)
	FCameraFeatureHandle Handle;
};
