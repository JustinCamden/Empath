// Copyright 2018 Team Empath All Rights Reserved

#include "EmpathTeleportBeacon.h"
#include "AI/Navigation/NavigationData.h"


// Sets default values
AEmpathTeleportBeacon::AEmpathTeleportBeacon()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void AEmpathTeleportBeacon::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void AEmpathTeleportBeacon::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

bool AEmpathTeleportBeacon::GetBestTeleportLocation_Implementation(FHitResult TeleportHit,
	FVector TeleportOrigin,
	FVector& OutTeleportLocation,
	ANavigationData* NavData,
	TSubclassOf<UNavigationQueryFilter> FilterClass) const
{
	return true;
}

