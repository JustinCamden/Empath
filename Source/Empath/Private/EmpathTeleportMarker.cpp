// Copyright 2018 Team Empath All Rights Reserved

#include "EmpathTeleportMarker.h"


// Sets default values
AEmpathTeleportMarker::AEmpathTeleportMarker()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void AEmpathTeleportMarker::BeginPlay()
{
	Super::BeginPlay();
	
}

// Called every frame
void AEmpathTeleportMarker::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}

void AEmpathTeleportMarker::UpdateMarkerLocation_Implementation(FVector TeleportLocation, 
	bool bLocationValid, 
	AEmpathCharacter* TargetedTeleportCharacter, 
	AEmpathTeleportBeacon* TargetedTeleportBeacon)
{
	// For now, just set our location to the teleport location
	SetActorLocation(TeleportLocation);
	return;
}