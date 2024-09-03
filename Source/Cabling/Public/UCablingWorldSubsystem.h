// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "FCablingRunner.h"
#include "HAL/Runnable.h"
#include "CablingCommonTypes.h"
#include "UCablingWorldSubsystem.generated.h"


//Goal: The Cabling Subsystem maintains the cabling thread and provides the output of
//the control polling that it performs to the normal input system. Cabling is not
//intended to replace a full input system, just provide a threaded flow

//This is not a full dispatch, as it possesses no ECS like capabilities to expose.
UCLASS()
class  CABLING_API UCablingWorldSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()
public:
	//in general, this should be called very seldom, as it is a DESTRUCTIVE
	//and slightly unsafe operation. calling it outside of postinitialize
	//or beginplay is not recommended. instead, clients should get a reference
	//and change what queue they listen on rather than replacing this queue.
	void DestructiveChangeLocalOutboundQueue(Cabling::SendQueue NewlyAllocatedQueue); 

protected:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void OnWorldBeginPlay(UWorld& InWorld) override;
	virtual void Deinitialize() override;
	virtual void PostInitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	//THIS IS THE SHARED SESSION ID. THIS IS NOT IMPLEMENTED YET.
	//THESE ARE ONLY GUARANTEED TO BE UNIQUE PER REFLECTOR ATM.
	//ULTIMATELY, THESE SHOULD BE THE CURRENT MATCH ID AS BRISTLECONE
	//SHOULD NOT BE RUNNING OUTSIDE OF A MATCH.

  private:
	

	// Receiver information

	FCabling controller_runner;
	Cabling::SendQueue GameThreadControlQueue;
	Cabling::SendQueue CabledThreadControlQueue;
	TUniquePtr<FRunnableThread> controller_thread;
};
