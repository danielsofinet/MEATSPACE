#include "Clankers/MsClankerFlyer.h"

#include "Combat/MsHealthComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "UObject/ConstructorHelpers.h"

AMsClankerFlyer::AMsClankerFlyer()
{
	MoveSpeed = 420.0f;
	TurnResponsiveness = 3.2f;

	// The flyer manages its own standoff distance, so the base class must not halt it.
	StopDistance = 0.0f;

	if (CollisionSphere)
	{
		CollisionSphere->InitSphereRadius(45.0f);
	}

	if (Mesh)
	{
		static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
		if (SphereMesh.Succeeded())
		{
			Mesh->SetStaticMesh(SphereMesh.Object);
		}
		Mesh->SetRelativeScale3D(FVector(0.8f, 0.8f, 0.8f));
	}

	if (HealthComponent)
	{
		// Squishier than the ground clankers - it survives by being hard to hit, not tanky.
		HealthComponent->MaxHealth = 40.0f;
		HealthComponent->Health = 40.0f;
	}
}

void AMsClankerFlyer::BeginPlay()
{
	Super::BeginPlay();

	// Desync multiple flyers so they never oscillate as a formation.
	PhaseOffset = FMath::FRandRange(0.0f, 2.0f * PI);
	RerollWander();
}

void AMsClankerFlyer::RerollWander()
{
	const UWorld* World = GetWorld();
	const float Now = World ? World->GetTimeSeconds() : 0.0f;

	WanderBias = FMath::FRandRange(-WanderStrength, WanderStrength);
	VerticalBias = FMath::FRandRange(-VerticalWanderRange, VerticalWanderRange);

	const float MinInterval = FMath::Min(MinWanderInterval, MaxWanderInterval);
	const float MaxInterval = FMath::Max(MinWanderInterval, MaxWanderInterval);
	NextWanderTime = Now + FMath::FRandRange(MinInterval, MaxInterval);
}

FVector AMsClankerFlyer::ComputeMoveDirection(float DeltaSeconds, const APawn* TargetPlayer)
{
	const UWorld* World = GetWorld();
	if (!World || !TargetPlayer)
	{
		return FVector::ZeroVector;
	}

	const float Now = World->GetTimeSeconds();
	if (Now >= NextWanderTime)
	{
		RerollWander();
	}

	const FVector MyLocation = GetActorLocation();
	const FVector PlayerLocation = TargetPlayer->GetActorLocation();

	FVector FlatToPlayer = PlayerLocation - MyLocation;
	FlatToPlayer.Z = 0.0f;

	const float FlatDistance = FlatToPlayer.Size();
	const FVector FlatDirection = FlatToPlayer.GetSafeNormal();

	// Radial: positive pulls in, negative pushes out. Zero at PreferredDistance, so it settles
	// into an orbit rather than charging or fleeing.
	const float DistanceError = FlatDistance - PreferredDistance;
	const float RadialAmount = FMath::Clamp(DistanceError / FMath::Max(PreferredDistance, 1.0f), -1.0f, 1.0f);
	const FVector Radial = FlatDirection * RadialAmount * RadialWeight;

	// Lateral: a sine slide plus a bias that re-rolls on an irregular clock.
	const FVector RightVector = FVector::CrossProduct(FVector::UpVector, FlatDirection).GetSafeNormal();
	const float Oscillation = FMath::Sin(Now * StrafeFrequency + PhaseOffset);
	const FVector Lateral = RightVector * (Oscillation + WanderBias) * StrafeWeight;

	// Vertical: hold station above the player, bob, and drift by the current bias.
	const float DesiredZ = PlayerLocation.Z + HoverHeight
		+ FMath::Sin(Now * BobFrequency + PhaseOffset) * BobAmplitude
		+ VerticalBias;

	const float HeightError = DesiredZ - MyLocation.Z;
	const FVector Vertical = FVector(0.0f, 0.0f,
		FMath::Clamp(HeightError / 150.0f, -1.0f, 1.0f)) * VerticalWeight;

	return (Radial + Lateral + Vertical).GetSafeNormal();
}
