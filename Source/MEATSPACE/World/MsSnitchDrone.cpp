#include "World/MsSnitchDrone.h"

#include "Character/MsCharacter.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"
#include "World/MsEncounterVolume.h"

#define LOCTEXT_NAMESPACE "Meatspace.Drone"

AMsSnitchDrone::AMsSnitchDrone()
{
	PrimaryActorTick.bCanEverTick = true;

	bReplicates = true;
	SetReplicateMovement(true);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetRootComponent(Mesh);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeMesh(TEXT("/Engine/BasicShapes/Cone.Cone"));
	if (ConeMesh.Succeeded())
	{
		Mesh->SetStaticMesh(ConeMesh.Object);
	}
	Mesh->SetRelativeScale3D(FVector(0.5f, 0.5f, 0.4f));

	AlertLine = LOCTEXT("DroneAlert", "Unauthorised blade detected. Reporting.");
}

void AMsSnitchDrone::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (BaseZ == 0.0f)
	{
		BaseZ = GetActorLocation().Z;
	}

	if (bFleeing)
	{
		FVector Location = GetActorLocation();
		Location.Z += FleeSpeed * DeltaSeconds;
		SetActorLocation(Location);

		// Gone once it is well clear.
		if (Location.Z > BaseZ + 4000.0f)
		{
			Destroy();
		}
		return;
	}

	// Idle hover, so it reads as a working machine rather than a prop.
	BobTime += DeltaSeconds;
	FVector Location = GetActorLocation();
	Location.Z = BaseZ + FMath::Sin(BobTime * HoverBobFrequency) * HoverBobAmplitude;
	SetActorLocation(Location);

	if (bSnitched || !HasAuthority())
	{
		return;
	}

	const APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!PlayerPawn)
	{
		return;
	}

	const AMsCharacter* PlayerCharacter = Cast<AMsCharacter>(PlayerPawn);

	// The sword is the problem. An unarmed player walks straight past, which is what makes
	// the fight feel caused rather than scripted.
	if (bOnlyIfArmed && (!PlayerCharacter || !PlayerCharacter->HasAnyWeapon()))
	{
		return;
	}

	if (FVector::Dist(PlayerPawn->GetActorLocation(), GetActorLocation()) <= NoticeRadius)
	{
		Snitch();
	}
}

void AMsSnitchDrone::Snitch()
{
	if (bSnitched)
	{
		return;
	}
	bSnitched = true;

	// Placeholder for a spoken bark. On screen for now so the beat is testable before audio
	// exists - but it is FText, so it is already translatable.
	if (GEngine && !AlertLine.IsEmpty())
	{
		GEngine->AddOnScreenDebugMessage(9300, 4.0f, FColor::Red, AlertLine.ToString());
	}

	OnSnitched.Broadcast(this);

	if (UWorld* World = GetWorld())
	{
		// The gap between being noticed and the dropships arriving. This is where the player
		// understands cause and effect - and gets a moment of dread.
		World->GetTimerManager().SetTimer(AlertTimer, this, &AMsSnitchDrone::CallEncounter,
			FMath::Max(AlertDelay, 0.01f), false);
	}
}

void AMsSnitchDrone::CallEncounter()
{
	if (EncounterToCall)
	{
		EncounterToCall->TriggerEncounter();
	}

	if (bFleeAfterSnitching)
	{
		bFleeing = true;
	}
}

#undef LOCTEXT_NAMESPACE
