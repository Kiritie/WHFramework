// Fill out your copyright notice in the Description page of Project Settings.

#pragma once


#include "Base/ProcedureBase.h"
#include "Main/Base/ModuleBase.h"

#include "ProcedureModule.generated.h"

class UProcedureBase;

/**
 * 
 */
UCLASS()
class WHFRAMEWORK_API UProcedureModule : public UModuleBase
{
	GENERATED_BODY()
		
	GENERATED_MODULE(UProcedureModule)

public:
	// ParamSets default values for this actor's properties
	UProcedureModule();

	~UProcedureModule();

	//////////////////////////////////////////////////////////////////////////
	/// Module
public:
	#if WITH_EDITOR
	virtual void OnGenerate() override;

	virtual void OnDestroy() override;
	#endif

	virtual void OnInitialize() override;

	virtual void OnPreparatory(EPhase InPhase) override;

	virtual void OnRefresh(float DeltaSeconds) override;

	virtual void OnPause() override;

	virtual void OnUnPause() override;

	virtual void OnTermination(EPhase InPhase) override;

	//////////////////////////////////////////////////////////////////////////
	/// ProcedureModule
protected:
	/// 自动切换初始流程 
	UPROPERTY(EditAnywhere, Category = "ProcedureModule")
	bool bAutoSwitchFirst;

public:
	UFUNCTION(BlueprintCallable)
	void SwitchProcedure(UProcedureBase* InProcedure);

	UFUNCTION(BlueprintCallable)
	void SwitchProcedureByIndex(int32 InIndex);

	template<class T>
	void SwitchProcedureByClass() { SwitchProcedureByClass(T::StaticClass()); }

	UFUNCTION(BlueprintCallable)
	void SwitchProcedureByClass(TSubclassOf<UProcedureBase> InClass);

	UFUNCTION(BlueprintCallable)
	void SwitchFirstProcedure();

	UFUNCTION(BlueprintCallable)
	void SwitchLastProcedure();

	UFUNCTION(BlueprintCallable)
	void SwitchNextProcedure();

	UFUNCTION(BlueprintCallable)
	void GuideCurrentProcedure();

	UFUNCTION(BlueprintCallable)
	void ClearAllProcedure();

	//////////////////////////////////////////////////////////////////////////
	/// Procedure Stats
protected:
	/// 初始流程 
	UPROPERTY(VisibleAnywhere, Category = "ProcedureModule|Procedure Stats")
	UProcedureBase* FirstProcedure;
	/// 当前流程 
	UPROPERTY(VisibleAnywhere, Transient, Category = "ProcedureModule|Procedure Stats")
	UProcedureBase* CurrentProcedure;
	/// 流程列表
	UPROPERTY(VisibleAnywhere, Category = "ProcedureModule|Procedure Stats")
	TArray<UProcedureBase*> Procedures;
	/// 流程Map
	UPROPERTY(VisibleAnywhere, Category = "ProcedureModule|Procedure Stats")
	TMap<TSubclassOf<UProcedureBase>, UProcedureBase*> ProcedureMap;

public:
	UFUNCTION(BlueprintPure)
	bool HasProcedureByIndex(int32 InIndex) const;

	template<class T>
	T* GetProcedureByIndex(int32 InIndex) const
	{
		if(HasProcedureByIndex(InIndex))
		{
			return Cast<T>(Procedures[InIndex]);
		}
		return nullptr;
	}
	
	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InIndex"))
	UProcedureBase* GetProcedureByIndex(int32 InIndex, TSubclassOf<UProcedureBase> InClass) const;
	
	template<class T>
	bool HasProcedureByClass(TSubclassOf<UProcedureBase> InClass = T::StaticClass()) const
	{
		if(!InClass) return false;

		return ProcedureMap.Contains(InClass);
	}
	
	UFUNCTION(BlueprintPure)
	bool HasProcedureByClass(TSubclassOf<UProcedureBase> InClass) const;

	template<class T>
	T* GetProcedureByClass(TSubclassOf<UProcedureBase> InClass = T::StaticClass()) const
	{
		if(!InClass) return nullptr;

		if(HasProcedureByClass<T>(InClass))
		{
			return Cast<T>(ProcedureMap[InClass]);
		}
		return nullptr;
	}
	
	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"))
	UProcedureBase* GetProcedureByClass(TSubclassOf<UProcedureBase> InClass) const;
		
	UFUNCTION(BlueprintPure)
	bool IsCurrentProcedure(UProcedureBase* InProcedure) const;

	UFUNCTION(BlueprintPure)
	bool IsCurrentProcedureIndex(int32 InIndex) const;

	template<class T>
	bool IsCurrentProcedureClass() const
	{
		return IsCurrentProcedureClass(T::StaticClass());
	}

	UFUNCTION(BlueprintPure)
	bool IsCurrentProcedureClass(TSubclassOf<UProcedureBase> InClass) const
	{
		if(!InClass) return false;

		return CurrentProcedure && CurrentProcedure->GetClass() == InClass;
	}

public:
	/**
	* 获取初始流程
	*/
	UFUNCTION(BlueprintPure)
	UProcedureBase* GetFirstProcedure() const { return FirstProcedure; }
	/**
	* 设置初始流程
	*/
	UFUNCTION(BlueprintCallable)
	void SetFirstProcedure(UProcedureBase* InFirstProcedure) { this->FirstProcedure = InFirstProcedure; }
	/**
	* 获取当前流程
	*/
	template<class T>
	T* GetCurrentProcedure() const
	{
		return Cast<T>(CurrentProcedure);
	}
	UFUNCTION(BlueprintPure, meta = (DeterminesOutputType = "InClass"))
	UProcedureBase* GetCurrentProcedure(TSubclassOf<UProcedureBase> InClass = nullptr) const { return CurrentProcedure; }
	/**
	* 获取流程列表
	*/
	UFUNCTION(BlueprintPure)
	TArray<UProcedureBase*>& GetProcedures() { return Procedures; }
	/**
	* 获取流程Map
	*/
	UFUNCTION(BlueprintPure)
	TMap<TSubclassOf<UProcedureBase>, UProcedureBase*>& GetProcedureMap() { return ProcedureMap; }

	//////////////////////////////////////////////////////////////////////////
	/// Editor
public:
	#if WITH_EDITOR
	void GenerateListItem(TArray<TSharedPtr<struct FProcedureListItem>>& OutProcedureListItems);

	void UpdateListItem(TArray<TSharedPtr<struct FProcedureListItem>>& OutProcedureListItems);
	#endif
};
