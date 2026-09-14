#pragma once
#include "UObject/Object.h"
#include "CameraDirectorBase.generated.h"

class ACameraManagerBase;

UCLASS(Abstract, Blueprintable)
class WHFRAMEWORK_API UCameraDirectorBase : public UObject
{
	GENERATED_BODY()

public:
	virtual void Start(ACameraManagerBase* InManager);

	virtual void Stop();

	virtual void Update(float DeltaTime);

	//////////////////////////////////////////////////////////////////////////
protected:
	UPROPERTY(Transient)
	TObjectPtr<ACameraManagerBase> CameraManager;
};
