#include "Clankers/MsClankerFlyer.h"

#include "Combat/MsHealthComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
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

void AMsClankerFlyer::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UWorld* World = GetWorld();
	if (!World || !HasAuthority() || !bCanShoot)
	{
		return;
	}

	if (HealthComponent && HealthComponent->IsDead())
	{
		return;
	}

	APawn* TargetPlayer = FindTargetPlayer();
	if (!TargetPlayer)
	{
		bTelegraphing = false;
		return;
	}

	const FVector MyLocation = GetActorLocation();
	const FVector TargetLocation = TargetPlayer->GetActorLocation();

	if (FVector::Dist(MyLocation, TargetLocation) > FireRange)
	{
		bTelegraphing = false;
		return;
	}

	const float Now = World->GetTimeSeconds();

	if (!bTelegraphing)
	{
		if (Now >= NextFireTime)
		{
			bTelegraphing = true;
			TelegraphEndTime = Now + TelegraphTime;
		}
	}
	else
	{
#if ENABLE_DRAW_DEBUG
		if (bDrawDebugShot)
		{
			// The wind-up must be visible or the attack is just unexplained damage. This is a
			// placeholder for a real charging effect.
			const float Charge = 1.0f - FMath::Clamp((TelegraphEndTime - Now) / FMath::Max(TelegraphTime, 0.01f), 0.0f, 1.0f);
			DrawDebugLine(World, MyLocation, TargetLocation,
				FColor(255, (uint8)(200 * (1.0f - Charge)), 40), false, -1.0f, 0, 1.0f + Charge * 3.0f);
		}
#endif

		if (Now >= TelegraphEndTime)
		{
			bTelegraphing = false;
			NextFireTime = Now + FireInterval;
			FireAtPlayer(TargetPlayer);
		}
	}
}

void AMsClankerFlyer::FireAtPlayer(APawn* TargetPlayer)
{
	UWorld* World = GetWorld();
	if (!World || !TargetPlayer)
	{
		return;
	}

	const FVector From = GetActorLocation();
	const FVector To = TargetPlayer->GetActorLocation();

	FCollisionQueryParams Params(SCENE_QUERY_STAT(MsFlyerShot), /*bTraceComplex=*/false);
	Params.AddIgnoredActor(this);

	// Traced rather than applied directly, so moving behind cover during the telegraph
	// actually saves you.
	FHitResult Hit;
	const bool bBlocked = World->LineTraceSingleByChannel(Hit, From, To, ECC_Visibility, Params);

	bool bHitPlayer = false;
	if (bBlocked && Hit.GetActor() == TargetPlayer)
	{
		bHitPlayer = true;
		UGameplayStatics::ApplyDamage(TargetPlayer, ShotDamage, nullptr, this, nullptr);
	}
	else if (!bBlocked)
	{
		// Nothing in the way at all - count it as a clean hit.
		bHitPlayer = true;
		UGameplayStatics::ApplyDamage(TargetPlayer, ShotDamage, nullptr, this, nullptr);
	}

	MulticastShotFX(From, bBlocked ? FVector(Hit.ImpactPoint) : To, bHitPlayer);
}

void AMsClankerFlyer::MulticastShotFX_Implementation(const FVector_NetQuantize& From,
	const FVector_NetQuantize& To, bool bHit)
{
#if ENABLE_DRAW_DEBUG
	if (!bDrawDebugShot)
	{
		return;
	}

	if (UWorld* World = GetWorld())
	{
		DrawDebugLine(World, FVector(From), FVector(To),
			bHit ? FColor(255, 80, 40) : FColor(200, 160, 60), false, 0.2f, 0, 4.0f);
	}
#endif
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
