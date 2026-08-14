#include "World/MsDropPod.h"

#include "Character/MsCharacter.h"
#include "Clankers/MsClankerBase.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

AMsDropPod::AMsDropPod()
{
	PrimaryActorTick.bCanEverTick = true;

	bReplicates = true;
	SetReplicateMovement(true);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetRootComponent(Mesh);

	// Placeholder shape until Daniel models a dropship.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderMesh.Succeeded())
	{
		Mesh->SetStaticMesh(CylinderMesh.Object);
	}
	Mesh->SetRelativeScale3D(FVector(1.4f, 1.4f, 2.2f));
}

void AMsDropPod::Deliver(const TArray<TSubclassOf<AMsClankerBase>>& Payload, const FVector& ImpactLocation)
{
	PendingPayload = Payload;
	TargetLocation = ImpactLocation;

	// Start high above the impact point and fall to it, so the drop is visible from a distance
	// and the player can see where it is going to land.
	SetActorLocation(ImpactLocation + FVector(0.0f, 0.0f, DropHeight));

	bFalling = true;
	bLanded = false;
}

void AMsDropPod::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bFalling || bLanded)
	{
		return;
	}

	FVector Location = GetActorLocation();
	Location.Z -= DropSpeed * DeltaSeconds;

	if (Location.Z <= TargetLocation.Z)
	{
		Location.Z = TargetLocation.Z;
		SetActorLocation(Location);
		Impact();
		return;
	}

	SetActorLocation(Location);
}

void AMsDropPod::Impact()
{
	bFalling = false;
	bLanded = true;

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Felt, not just seen. Scaled by distance so a pod landing across the street registers
	// but does not rattle the camera as hard as one landing next to you.
	if (APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		if (AMsCharacter* PlayerCharacter = Cast<AMsCharacter>(PlayerPawn))
		{
			const float Distance = FVector::Dist(PlayerCharacter->GetActorLocation(), GetActorLocation());
			const float Falloff = 1.0f - FMath::Clamp(Distance / FMath::Max(ShakeFalloffDistance, 1.0f), 0.0f, 1.0f);

			if (Falloff > 0.0f)
			{
				PlayerCharacter->AddCameraShake(ImpactShake * Falloff);
			}
		}
	}

#if ENABLE_DRAW_DEBUG
	if (bDrawDebugImpact)
	{
		DrawDebugSphere(World, GetActorLocation(), 260.0f, 16, FColor(255, 160, 40), false, 1.2f, 0, 3.0f);
	}
#endif

	// The beat between landing and unloading. This is the moment the player reads what has
	// arrived and decides whether to close in or back off.
	World->GetTimerManager().SetTimer(OpenTimer, this, &AMsDropPod::ReleasePayload,
		FMath::Max(OpenDelay, 0.01f), false);
}

void AMsDropPod::ReleasePayload()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FVector Origin = GetActorLocation();
	const int32 Count = PendingPayload.Num();

	for (int32 Index = 0; Index < Count; ++Index)
	{
		const TSubclassOf<AMsClankerBase> ClankerClass = PendingPayload[Index];
		if (!ClankerClass)
		{
			continue;
		}

		// Fan them out evenly around the pod rather than stacking them on one point.
		const float Angle = (2.0f * PI * Index) / FMath::Max(Count, 1);
		const FVector Offset(FMath::Cos(Angle) * PayloadSpread, FMath::Sin(Angle) * PayloadSpread, 0.0f);

		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		SpawnParams.Owner = this;

		if (AMsClankerBase* Clanker = World->SpawnActor<AMsClankerBase>(
			ClankerClass, Origin + Offset, Offset.Rotation(), SpawnParams))
		{
			OnClankerDelivered.Broadcast(Clanker);
		}
	}

	PendingPayload.Reset();

	if (DespawnDelay > 0.0f)
	{
		SetLifeSpan(DespawnDelay);
	}
}
