// Copyright 2018 Team Empath All Rights Reserved

#include "EmpathNavigationLayerBase.h"


// Sets default values
AEmpathNavigationLayerBase::AEmpathNavigationLayerBase()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void AEmpathNavigationLayerBase::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void AEmpathNavigationLayerBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

