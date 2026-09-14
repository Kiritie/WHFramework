#include "Camera/Actor/CameraRigBase.h"
#include "Camera/CameraComponent.h"
#include "Camera/Components/CameraSpringArmComponent.h"

ACameraRigBase::ACameraRigBase()
{
	PrimaryActorTick.bCanEverTick = false;
	CameraBoom = CreateDefaultSubobject<UCameraSpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->bUsePawnControlRotation = false;
	CameraBoom->bDoCollisionTest = true;
	CameraBoom->TargetArmLength = 0.f;
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	Camera->bUsePawnControlRotation = false;
}
