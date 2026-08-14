#include "Clankers/MsClankerSmall.h"

#include "Combat/MsHealthComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/Pawn.h"

TArray<TWeakObjectPtr<AMsClankerSmall>> AMsClankerSmall::LiveClankers;

AMsClankerSmall::AMsClankerSmall()
{
	// Fast and light - they should feel like they are scuttling at you, not marching.
	MoveSpeed = 480.0f;
	TurnResponsiveness = 6.0f;
	StopDistance = 95.0f;

	bSnapToGround = true;
	GroundOffset = 38.0f;

	if (CollisionSphere)
	{
		CollisionSphere->InitSphereRadius(35.0f);
	}

	if (Mesh)
	{
		// Engine cube is 100 units - scale it down to a small, scuttling thing.
		Mesh->SetRelativeScale3D(FVector(0.6f, 0.6f, 0.6f));
		Mesh->SetRelativeLocation(FVector(0.0f, 0.0f, -5.0f));
	}

	if (HealthComponent)
	{
		// One shot from anything. These are threatening by number, not by durability - and a
		// swarm you can carve through is far more satisfying than one you have to grind.
		HealthComponent->MaxHealth = 10.0f;
		HealthComponent->Health = 10.0f;
	}
}

void AMsClankerSmall::BeginPlay()
{
	Super::BeginPlay();

	LiveClankers.Add(this);
}

void AMsClankerSmall::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	LiveClankers.RemoveAll([this](const TWeakObjectPtr<AMsClankerSmall>& Entry)
	{
		return !Entry.IsValid() || Entry.Get() == this;
	});

	Super::EndPlay(EndPlayReason);
}

FVector AMsClankerSmall::ComputeMoveDirection(float DeltaSeconds, const APawn* TargetPlayer)
{
	const FVector MyLocation = GetActorLocation();

	FVector Seek = FVector::ZeroVector;
	if (TargetPlayer)
	{
		FVector ToPlayer = TargetPlayer->GetActorLocation() - MyLocation;
		ToPlayer.Z = 0.0f;
		Seek = ToPlayer.GetSafeNormal();
	}

	FVector Separation = FVector::ZeroVector;
	FVector CentreSum = FVector::ZeroVector;
	FVector HeadingSum = FVector::ZeroVector;
	int32 NeighbourCount = 0;

	const float NeighbourRadiusSq = FMath::Square(NeighbourRadius);
	const float SeparationRadiusSq = FMath::Square(SeparationRadius);

	for (const TWeakObjectPtr<AMsClankerSmall>& Entry : LiveClankers)
	{
		const AMsClankerSmall* Other = Entry.Get();
		if (!Other || Other == this)
		{
			continue;
		}

		if (Other->HealthComponent && Other->HealthComponent->IsDead())
		{
			continue;
		}

		FVector Offset = Other->GetActorLocation() - MyLocation;
		Offset.Z = 0.0f;
		const float DistSq = Offset.SizeSquared();

		if (DistSq > NeighbourRadiusSq || DistSq <= KINDA_SMALL_NUMBER)
		{
			continue;
		}

		++NeighbourCount;
		CentreSum += Other->GetActorLocation();
		HeadingSum += Other->CurrentVelocity;

		if (DistSq < SeparationRadiusSq)
		{
			// Push harder the closer they are - this is what stops them merging into one cube.
			const float Distance = FMath::Sqrt(DistSq);
			Separation += (-Offset / Distance) * (1.0f - Distance / SeparationRadius);
		}
	}

	FVector Cohesion = FVector::ZeroVector;
	FVector Alignment = FVector::ZeroVector;

	if (NeighbourCount > 0)
	{
		FVector Centre = CentreSum / NeighbourCount;
		FVector ToCentre = Centre - MyLocation;
		ToCentre.Z = 0.0f;
		Cohesion = ToCentre.GetSafeNormal();

		FVector AverageHeading = HeadingSum / NeighbourCount;
		AverageHeading.Z = 0.0f;
		Alignment = AverageHeading.GetSafeNormal();
	}

	const FVector Steering =
		Seek * SeekWeight +
		Separation.GetSafeNormal() * SeparationWeight +
		Cohesion * CohesionWeight +
		Alignment * AlignmentWeight;

	FVector Result = Steering;
	Result.Z = 0.0f;

	return Result.GetSafeNormal();
}
