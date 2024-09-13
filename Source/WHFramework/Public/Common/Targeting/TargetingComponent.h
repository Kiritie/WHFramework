// Copyright 2018-2021 Mickael Daniel. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Components/ActorComponent.h"
#include "TargetingComponent.generated.h"

class UWorldWidgetBase;
class UUserWidget;
class UWidgetComponent;
class APlayerController;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FComponentOnTargetLockedOnOff, AActor*, TargetActor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FComponentSetRotation, AActor*, TargetActor, FRotator, ControlRotation);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class WHFRAMEWORK_API UTargetingComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UTargetingComponent();

	// The minimum distance to enable target locked on.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting")
	float MinimumDistanceToEnable = 1200.0f;

	// AActor子类，用于搜索目标actor
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting")
	TSubclassOf<AActor> TargetableActors;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting")
	TEnumAsByte<ECollisionChannel> TargetableCollisionChannel;

	// 当目标被锁定时，角色旋转是否应该被控制
	//
	// 如果为true，它将设置目标锁定上的busecontrolllerrotationyaw和borientationtommovement变量的值
	//
	// 如果你想让角色围绕锁定的目标旋转，将其设置为true，以便设置策略动画
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting")
	bool bShouldControlRotation = false;

	// 当bAdjustPitchBasedOnDistanceToTarget被禁用时，是否接受pitch输入
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting")
	bool bIgnoreLookInput = true;

	// 当演员走到一个物体后面时，脱离视线的时间
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting")
	float BreakLineOfSightDelay = 2.0f;

	// 该值越小，在右侧或左侧切换新目标就越容易。如果用摇杆控制必须< 1.0f
	//
	// 当使用粘性感觉功能时，它没有效果(参见StickyRotationThreshold)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting")
	float StartRotatingThreshold = 0.85f;

	// 旋转平滑速度
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting")
	float RotatingSmoothnessSpeed = 9.f;

	// Target LockOn Widget指示器是否应该自动绘制和附加
	//
	// 当设置为false时，这允许您手动绘制小部件，以进一步控制您希望它出现的位置
	//
	// OnTargetLockedOn和OnTargetLockedOff事件可以用于此
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting|Widget")
	bool bShouldDrawLockedOnWidget = true;

	// 锁定目标时要使用的小部件类。如果没有定义，将退回到文本呈现
	// 带有单个0字符的小部件
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting|Widget")
	TSubclassOf<UWorldWidgetBase> LockedOnWidgetClass;

	// 附加LockedOn小部件的套接字名称
	//
	// 您应该使用它来配置小部件应该附加到的Bone或Socket名称，并允许
	// 随着目标角色的动画移动的小部件(例如:spine_03)
	//
	// 将其设置为None以将Widget组件附加到根组件而不是网格
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting|Widget")
	FName LockedOnWidgetParentSocket = FName("spine_03");

	// 当附加到目标时，应用于Target LockedOn小部件的相对位置
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting|Widget")
	FVector LockedOnWidgetRelativeLocation = FVector(0.0f, 0.0f, 0.0f);

	// 将此设置为true将告诉Targeting在锁定时调整Pitch Offset (Y轴)
	// 取决于与目标演员的距离
	//
	// 它将确保当Actor接近其目标时，摄像机将垂直向上移动
	//
	// 公式:
	//
	//   (DistanceToTarget * PitchDistanceCoefficient + PitchDistanceOffset) * -1.0f
	//
	// 由 PitchMin / PitchMax 限制
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting|Pitch Offset")
	bool bAdjustPitchBasedOnDistanceToTarget = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting|Pitch Offset")
	float PitchDistanceCoefficient = -0.2f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting|Pitch Offset")
	float PitchDistanceOffset = 60.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting|Pitch Offset")
	float PitchMin = -50.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting|Pitch Offset")
	float PitchMax = -20.0f;

	// 在切换目标时是否需要粘性感，请将其设置为true / false
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting|Sticky Feeling on Target Switch")
	bool bEnableStickyTarget = false;

	// 该值将乘以AxisValue，以对照StickyRotationThreshold进行检查
	//
	// 仅在启用粘接目标时使用
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting|Sticky Feeling on Target Switch")
	float AxisMultiplier = 1.0f;

	// 该值越小，在右侧或左侧切换新目标就越容易
	//
	// 这与StartRotatingThreshold类似，但您应该将其设置为更高的值
	//
	// 仅在启用粘接目标时使用
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Targeting|Sticky Feeling on Target Switch")
	float StickyRotationThreshold = 30.0f;

	// 函数调用以定位新参与者
	UFUNCTION(BlueprintCallable, Category = "Targeting")
	void TargetActor();
	
	// 函数调用以手动解除目标
	UFUNCTION(BlueprintCallable, Category = "Targeting")
	void TargetLockOff();

	UFUNCTION(BlueprintCallable, Category = "Targeting")
	void SetShouldControlRotation(bool bValue);

	/**
	* 函数调用来切换x轴鼠标/控制器摇杆的移动
	*
	* @param AxisValue 传入输入轴的浮点值
	*/
	UFUNCTION(BlueprintCallable, Category = "Targeting")
	void TargetActorWithAxisInput(float AxisValue);

	// 获取TargetLocked私有变量状态的函数
	UFUNCTION(BlueprintCallable, Category = "Targeting")
	bool GetTargetLockedStatus();

	// 当目标被锁定时调用，如果它在到达之外(基于MinimumDistanceToEnable)或在对象后面
	UPROPERTY(BlueprintAssignable, Category = "Targeting")
	FComponentOnTargetLockedOnOff OnTargetLockedOff;

	// 锁定目标时调用
	UPROPERTY(BlueprintAssignable, Category = "Targeting")
	FComponentOnTargetLockedOnOff OnTargetLockedOn;

	// when a target is locked on
	//
	// 如果没有实现，将回退到默认实现
	// 如果实现了这个事件，你就可以控制角色的旋转
	UPROPERTY(BlueprintAssignable, Category = "Targeting")
	FComponentSetRotation OnTargetSetRotation;

	// 如果有的话，返回对当前目标Actor的引用
	UFUNCTION(BlueprintCallable, Category = "Targeting")
	AActor* GetLockedOnTargetActor() const;

	// 返回true / false系统是否针对参与者
	UFUNCTION(BlueprintCallable, Category = "Targeting")
	bool IsLocked() const;

private:
	UPROPERTY()
	AActor* OwnerActor;

	UPROPERTY()
	APawn* OwnerPawn;

	UPROPERTY()
	APlayerController* OwnerPlayerController;

	UPROPERTY()
	UWorldWidgetBase* TargetLockedOnWidget;

	UPROPERTY()
	AActor* LockedOnTargetActor;

	FTimerHandle LineOfSightBreakTimerHandle;
	FTimerHandle SwitchingTargetTimerHandle;

	bool bIsBreakingLineOfSight = false;
	bool bIsSwitchingTarget = false;
	bool bTargetLocked = false;
	float ClosestTargetDistance = 0.0f;

	bool bDesireToSwitch = false;
	float StartRotatingStack = 0.0f;

	//~ Actors search / trace

	TArray<AActor*> GetAllActorsOfClass(TSubclassOf<AActor> ActorClass) const;
	TArray<AActor*> FindTargetsInRange(TArray<AActor*> ActorsToLook, float RangeMin, float RangeMax) const;

	AActor* FindNearestTarget(TArray<AActor*> Actors) const;

	bool LineTrace(FHitResult& HitResult, const AActor* OtherActor, TArray<AActor*> ActorsToIgnore = TArray<AActor*>()) const;
	bool LineTraceForActor(AActor* OtherActor, TArray<AActor*> ActorsToIgnore = TArray<AActor*>()) const;

	bool ShouldBreakLineOfSight() const;
	void BreakLineOfSight();

	bool IsInViewport(const AActor* TargetActor) const;

	float GetDistanceFromCharacter(const AActor* OtherActor) const;


	//~ Actor rotation

	FRotator GetControlRotationOnTarget(const AActor* OtherActor) const;
	void SetControlRotationOnTarget(AActor* TargetActor) const;
	void ControlRotation(bool ShouldControlRotation) const;

	float GetAngleUsingCameraRotation(const AActor* ActorToLook) const;
	float GetAngleUsingCharacterRotation(const AActor* ActorToLook) const;

	static FRotator FindLookAtRotation(const FVector Start, const FVector Target);

	//~ Widget

	void CreateAndAttachTargetLockedOnWidgetComponent(AActor* TargetActor);

	//~ Targeting

	void TargetLockOn(AActor* TargetToLockOn);
	void ResetIsSwitchingTarget();
	bool ShouldSwitchTargetActor(float AxisValue);

	bool TargetIsTargetAble(const AActor* Actor) const;

	/**
	 *  从Owner Pawn设置缓存的Owner PlayerController
	 *
	 *  对于本地分屏，当这个组件开始播放时，Pawn的控制器可能还没有设置好
	 */
	 void SetupLocalPlayerController();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
};
