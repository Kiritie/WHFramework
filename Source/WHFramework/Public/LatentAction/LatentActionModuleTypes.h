// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"

#include "LatentActions.h"
#include "UObject/WeakObjectPtr.h"
#include "LatentActionModuleTypes.generated.h"

//////////  MoveActorToLatentAction ///////////////

UENUM(BlueprintType)
namespace EMoveActorAction
{
	enum Type
	{
		Start,
		Stop,
		Return,
	};
}

class WHFRAMEWORK_API FMoveActorToAction : public FPendingLatentAction
{
	public:
    	/** 应该发生插值的时间 */
    	float TotalTime;
    	/** 到目前为止的插值时间 */
    	float TimeElapsed;
    	/** 是否进行插值 */
    	bool bInterpolating;
    	/** 完成时执行的任务 */
    	FName ExecutionFunction;
    	/** 完成时触发的链接 */
    	int32 OutputLink;
    	/** 完成后调用回调的对象  */
    	FWeakObjectPtr CallbackTarget;
    	/** 需要进行插值的目标 */
    	TWeakObjectPtr<AActor> Target;
    
		/** 是否更新位置 */
		bool bInterpLocation;
		/** 插值目标的位置 */
		FVector InitialLocation;
		/** 去到的位置 */
		FVector TargetLocation;
	
    	/** 是否更新旋转值 */
    	bool bInterpRotation;
    	/** 插值目标的旋转 */
    	FRotator InitialRotation;
    	/** 去到的旋转 */
    	FRotator TargetRotation;
    
		/** 是否更新缩放 */
	    bool bInterpScale;
		/** 插值目标的缩放 */
		FVector InitialScale;
		/** 去到的缩放 */
		FVector TargetScale;

		/** 是否淡入 */
		bool bEaseIn;
		/** 是否淡出 */
		bool bEaseOut;
		/** 混合百分比 */
		float BlendExp;
    	/** 是否强制使用最短路径进行旋转 */
    	bool bForceShortestRotationPath;
    
    	FMoveActorToAction(float Duration, const FLatentActionInfo& LatentInfo, AActor* Object, bool bUseRotator, bool bUseScale, bool bEaseIn, bool bEaseOut, float BlendExp, bool bInForceShortestRotationPath)
    		: TotalTime(Duration)
    		, TimeElapsed(0.f)
    		, bInterpolating(true)
    		, ExecutionFunction(LatentInfo.ExecutionFunction)
    		, OutputLink(LatentInfo.Linkage)
    		, CallbackTarget(LatentInfo.CallbackTarget)
    		, Target(Object)
			, bInterpLocation(true)
			, InitialLocation(FVector::ZeroVector)
			, TargetLocation(FVector::ZeroVector)
    		, bInterpRotation(bUseRotator)
    		, InitialRotation(FRotator::ZeroRotator)
    		, TargetRotation(FRotator::ZeroRotator)
			, bInterpScale(bUseScale)
			, InitialScale(FVector::ZeroVector)
			, TargetScale(FVector::ZeroVector)
			, bEaseIn(bEaseIn)
			, bEaseOut(bEaseOut)
			, BlendExp(BlendExp)
    		, bForceShortestRotationPath(bInForceShortestRotationPath)
    	{
    	}
    
    	virtual void UpdateOperation(FLatentResponse& Response) override
    	{
    		// Update elapsed time
    		TimeElapsed += Response.ElapsedTime();
            const bool bComplete = (TimeElapsed >= TotalTime);

            if (bComplete)
            {
	            bInterpolating = false;
            	
	            if (Target.IsValid())
	            {
		            if (bInterpLocation)
		            {
		            	Target->SetActorLocation(TargetLocation);
		            }
	            	if (bInterpRotation)
	            	{
	            		Target->SetActorRotation(TargetRotation);
	            	}
	                if (bInterpScale)
	                {
	                	Target->SetActorScale3D(TargetScale);
	                }
	            }
            	
				// #if WITH_EDITOR
				// WH_LOG(LogTemp,Log,TEXT("插值变换结束."));
				// #endif
            }
    		
    		// 如果插值目标存在
    		if(Target.IsValid() && bInterpolating)
    		{
	            const float DurationPct = TimeElapsed/TotalTime;
    			float BlendPct;
    			
    			if(bEaseIn)
    			{
    				if(bEaseOut)
    				{
    					BlendPct = FMath::InterpEaseInOut(0.f, 1.f, DurationPct, BlendExp);
    				}
    				else
    				{
    					BlendPct = FMath::Lerp(0.f, 1.f, FMath::Pow(DurationPct, BlendExp));
    				}
    			}
    			else
    			{
    				if(bEaseOut)
    				{
    					BlendPct = FMath::Lerp(0.f, 1.f, FMath::Pow(DurationPct, 1.f / BlendExp));
    				}
    				else
    				{
    					BlendPct = FMath::Lerp(0.f, 1.f, DurationPct);
    				}
    			}
    
    			// 更新位置信息
    			if(bInterpLocation)
    			{
	                const FVector NewLocation = bComplete ? TargetLocation : FMath::Lerp(InitialLocation, TargetLocation, BlendPct);
    				Target->SetActorLocation(NewLocation, false);
    			}
    
    			if(bInterpRotation)
    			{
    				FRotator NewRotation;
    				// If we are done just set the final rotation
    				if (bComplete)
    				{
    					NewRotation = TargetRotation;
    				}
    				else if (bForceShortestRotationPath)
    				{
	                    const FQuat AQuat(InitialRotation);
	                    const FQuat BQuat(TargetRotation);
    
    					FQuat Result = FQuat::Slerp(AQuat, BQuat, BlendPct);
    					Result.Normalize();
    					NewRotation = Result.Rotator();
    				}
    				else
    				{
    					// dont care about it being the shortest path - just lerp
    					NewRotation = FMath::Lerp(InitialRotation, TargetRotation, BlendPct);
    				}
    				Target->SetActorRotation(NewRotation, ETeleportType::None);
    			}
                if (bInterpScale)
                {
                	FVector NewScale3D;
	                if (bComplete)
	                {
	                	NewScale3D = TargetScale;
	                }
                	else
	                {
                		NewScale3D = FMath::Lerp(InitialScale, TargetScale, BlendPct);
	                }
                	Target->SetActorScale3D(NewScale3D);
                }
    		}
    		
    		Response.FinishAndTriggerIf(bComplete || !bInterpolating, ExecutionFunction, OutputLink, CallbackTarget);
    	}
    
    #if WITH_EDITOR
    	virtual FString GetDescription() const override
    	{
    		static const FNumberFormattingOptions DelayTimeFormatOptions = FNumberFormattingOptions()
    			.SetMinimumFractionalDigits(3)
    			.SetMaximumFractionalDigits(3);
    		 return FText::Format(NSLOCTEXT("FMoveActorToAction", "FMoveActorToActionTimeFmt", "Move ({0} seconds left)"), FText::AsNumber(TotalTime-TimeElapsed, &DelayTimeFormatOptions)).ToString();
    	}
    #endif
	
};

class WHFRAMEWORK_API FRotatorActorToAction : public FPendingLatentAction
{
	public:
    	/** 应该发生插值的时间 */
    	float TotalTime;
    	/** 到目前为止的插值时间 */
    	float TimeElapsed;
    	/** 是否进行插值 */
    	bool bInterpolating;
    	/** 完成时执行的任务 */
    	FName ExecutionFunction;
    	/** 完成时触发的链接 */
    	int32 OutputLink;
    	/** 完成后调用回调的对象  */
    	FWeakObjectPtr CallbackTarget;
    	/** 需要进行插值的目标 */
    	TWeakObjectPtr<AActor> Target;
    
    	/** 插值目标的旋转 */
    	FRotator InitialRotation;
    	/** 去到的旋转 */
    	FRotator TargetRotation;

    	FRotatorActorToAction(float Duration, const FLatentActionInfo& LatentInfo, AActor* Object)
	        : TotalTime(Duration)
	          , TimeElapsed(0.f)
	          , bInterpolating(true)
	          , ExecutionFunction(LatentInfo.ExecutionFunction)
	          , OutputLink(LatentInfo.Linkage)
	          , CallbackTarget(LatentInfo.CallbackTarget)
	          , Target(Object)
	          , InitialRotation(FRotator::ZeroRotator)
	          , TargetRotation(FRotator::ZeroRotator)
        {
        }

    	virtual void UpdateOperation(FLatentResponse& Response) override
    	{
    		// Update elapsed time
    		TimeElapsed += Response.ElapsedTime();
            const bool bComplete = (TimeElapsed >= TotalTime);

    		if (bComplete)
    		{
    			bInterpolating = false;
    			if (Target.IsValid())
    			{
    				Target->SetActorRotation(TargetRotation);
    			}
    		}
    		
    		// 如果插值目标存在
    		if(bInterpolating)
    		{
	            if(Target.IsValid())
    			{
	                FRotator NewRotation;
    				if (bComplete)
    				{
    					NewRotation = TargetRotation;
    				}
    				else
    				{
    					const float DurationPct = TimeElapsed/TotalTime;
    					const float BlendPct = FMath::Lerp(0.f, 1.f, DurationPct);
    					NewRotation = FMath::Lerp(InitialRotation, TargetRotation, BlendPct);
    				}
    				Target->SetActorRotation(NewRotation, ETeleportType::None);
    			}
    		}
    		
    		Response.FinishAndTriggerIf(bComplete || !bInterpolating, ExecutionFunction, OutputLink, CallbackTarget);
    	}
	
    
#if WITH_EDITOR
virtual FString GetDescription() const override
	{
    		static const FNumberFormattingOptions DelayTimeFormatOptions = FNumberFormattingOptions()
				.SetMinimumFractionalDigits(3)
				.SetMaximumFractionalDigits(3);
    		return FText::Format(NSLOCTEXT("FMoveActorToAction", "FMoveActorToActionTimeFmt", "Move ({0} seconds left)"), FText::AsNumber(TotalTime-TimeElapsed, &DelayTimeFormatOptions)).ToString();
	}
#endif
	
};

class WHFRAMEWORK_API FScaleActorToAction : public FPendingLatentAction
{
	public:
    	/** 应该发生插值的时间 */
    	float TotalTime;
    	/** 到目前为止的插值时间 */
    	float TimeElapsed;
    	/** 是否进行插值 */
    	bool bInterpolating;
    	/** 完成时执行的任务 */
    	FName ExecutionFunction;
    	/** 完成时触发的链接 */
    	int32 OutputLink;
    	/** 完成后调用回调的对象  */
    	FWeakObjectPtr CallbackTarget;
    	/** 需要进行插值的目标 */
    	TWeakObjectPtr<AActor> Target;
    
    	/** 插值目标的旋转 */
    	FVector InitialScale;
    	/** 去到的旋转 */
    	FVector TargetScale;

    	FScaleActorToAction(float Duration, const FLatentActionInfo& LatentInfo, AActor* Object)
    		: TotalTime(Duration)
    		, TimeElapsed(0.f)
    		, bInterpolating(true)
    		, ExecutionFunction(LatentInfo.ExecutionFunction)
    		, OutputLink(LatentInfo.Linkage)
    		, CallbackTarget(LatentInfo.CallbackTarget)
    		, Target(Object)
    		, InitialScale(FVector::ZeroVector)
    		, TargetScale(FVector::ZeroVector)
    	{
    	}
    
    	virtual void UpdateOperation(FLatentResponse& Response) override
    	{
    		// Update elapsed time
    		TimeElapsed += Response.ElapsedTime();
            const bool bComplete = (TimeElapsed >= TotalTime);
    		
    		// 如果插值目标存在
    		if(Target.IsValid() && bInterpolating)
    		{
	            FVector NewScale3D;
    			if (bComplete)
    			{
    				NewScale3D = TargetScale;
    			}
    			else
    			{
    				const float DurationPct = TimeElapsed/TotalTime;
    				const float BlendPct = FMath::Lerp(0.f, 1.f, DurationPct);
    				NewScale3D = FMath::Lerp(InitialScale, TargetScale, BlendPct);
    			}
    			Target->SetActorScale3D(NewScale3D);
    		}
    		
    		Response.FinishAndTriggerIf(bComplete || !bInterpolating, ExecutionFunction, OutputLink, CallbackTarget);
    	}
    	
    
#if WITH_EDITOR
virtual FString GetDescription() const override
	{
    		static const FNumberFormattingOptions DelayTimeFormatOptions = FNumberFormattingOptions()
				.SetMinimumFractionalDigits(3)
				.SetMaximumFractionalDigits(3);
    		return FText::Format(NSLOCTEXT("FScaleActorToAction", "FScaleActorToActionTimeFmt", "Move ({0} seconds left)"), FText::AsNumber(TotalTime-TimeElapsed, &DelayTimeFormatOptions)).ToString();
	}
#endif
	
};


//////////  CancelableDelayLatentAction ///////////////

UENUM(BlueprintType)
namespace ECancelableDelayAction
{
	enum Type
	{	
		None,
		Cancel
	};
}

class WHFRAMEWORK_API FCancelableDelayAction : public FPendingLatentAction
{
	public:
	float TimeRemaining;
	bool  bExecute;
	FName ExecutionFunction;
	int32 OutputLink;
	FWeakObjectPtr CallbackTarget;

	FCancelableDelayAction(float Duration, const FLatentActionInfo& LatentInfo)
		: TimeRemaining(Duration)
		, bExecute(true)
		, ExecutionFunction(LatentInfo.ExecutionFunction)
		, OutputLink(LatentInfo.Linkage)
		, CallbackTarget(LatentInfo.CallbackTarget)
	{
	}

	virtual void UpdateOperation(FLatentResponse& Response) override
	{
		TimeRemaining -= Response.ElapsedTime();
		Response.FinishAndTriggerIf(TimeRemaining <= 0.0f && bExecute, ExecutionFunction, OutputLink, CallbackTarget);
	}

#if WITH_EDITOR
	// Returns a human readable description of the latent operation's current state
	virtual FString GetDescription() const override
	{
		static const FNumberFormattingOptions CancelableDelayTimeFormatOptions = FNumberFormattingOptions()
			.SetMinimumFractionalDigits(3)
			.SetMaximumFractionalDigits(3);
		return FText::Format(NSLOCTEXT("CancelableDelayAction", "CancelableDelayActionTimeFmt", "Delay ({0} seconds left)"), FText::AsNumber(TimeRemaining, &CancelableDelayTimeFormatOptions)).ToString();
	}
#endif
};
