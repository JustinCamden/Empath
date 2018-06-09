// Copyright 2018 Team Empath All Rights Reserved

#include "TeleportBeacon.h"


// Sets default values
ATeleportBeacon::ATeleportBeacon()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void ATeleportBeacon::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void ATeleportBeacon::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

