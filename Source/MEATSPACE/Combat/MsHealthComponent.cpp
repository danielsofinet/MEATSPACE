#include "Combat/MsHealthComponent.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"

UMsHealthComponent::UMsHealthComponent()
{
	// Ticks only to regenerate shield; enabled in BeginPlay when there is a shield to regen.
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;

	SetIsReplicatedByDefault(true);
}

void UMsHealthComponent::BeginPlay()
{
	Super::BeginPlay();

	Health = MaxHealth;
	LastHealth = Health;

	Shield = MaxShield;
	LastShield = Shield;

	AActor* Owner = GetOwner();
	const bool bAuthority = Owner && Owner->HasAuthority();

	// Only the server listens for damage - it is the only one allowed to change health.
	if (bAuthority)
	{
		Owner->OnTakeAnyDamage.AddDynamic(this, &UMsHealthComponent::HandleTakeAnyDamage);
	}

	// No shield, no tick. Keeps the cost at zero for the hundreds of clankers that will
	// never have one.
	SetComponentTickEnabled(bAuthority && HasShield());
}

void UMsHealthComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UMsHealthComponent, Health);
	DOREPLIFETIME(UMsHealthComponent, Shield);
}

void UMsHealthComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const UWorld* World = GetWorld();
	if (!World || !HasShield() || IsDead())
	{
		return;
	}

	if (Shield >= MaxShield)
	{
		return;
	}

	// Regeneration is gated on not having been hit recently, so the shield rewards
	// disengaging rather than quietly topping up while you stand in the swarm.
	if (World->GetTimeSeconds() < LastDamageTime + ShieldRegenDelay)
	{
		return;
	}

	Shield = FMath::Min(Shield + ShieldRegenRate * DeltaTime, MaxShield);

	const float Delta = Shield - LastShield;
	LastShield = Shield;

	OnShieldChanged.Broadcast(Shield, Delta);
}

void UMsHealthComponent::HandleTakeAnyDamage(AActor* DamagedActor, float DamageAmount,
	const UDamageType* DamageType, AController* InstigatedBy, AActor* DamageCauser)
{
	if (DamageAmount <= 0.0f || IsDead())
	{
		return;
	}

	if (const UWorld* World = GetWorld())
	{
		LastDamageTime = World->GetTimeSeconds();
	}

	float DamageToHealth = DamageAmount;

	if (HasShield() && Shield > 0.0f)
	{
		// Split the hit. The shield takes its share, capped by what it has left; whatever the
		// shield could not cover still reaches health. A depleted shield stops mattering
		// immediately rather than lingering as partial protection.
		const float Absorbed = FMath::Min(Shield, DamageAmount * ShieldAbsorbFraction);

		Shield = FMath::Max(Shield - Absorbed, 0.0f);
		DamageToHealth = DamageAmount - Absorbed;

		const float ShieldDelta = Shield - LastShield;
		LastShield = Shield;
		OnShieldChanged.Broadcast(Shield, ShieldDelta);
	}

	Health = FMath::Max(Health - DamageToHealth, 0.0f);

	const float Delta = Health - LastHealth;
	LastHealth = Health;

	// OnRep does not fire on the server, so broadcast here as well.
	OnHealthChanged.Broadcast(Health, Delta);

	if (Health <= 0.0f)
	{
		OnDeath.Broadcast(GetOwner());
	}
}

void UMsHealthComponent::OnRep_Health()
{
	const float Delta = Health - LastHealth;
	LastHealth = Health;

	OnHealthChanged.Broadcast(Health, Delta);
}

void UMsHealthComponent::OnRep_Shield()
{
	const float Delta = Shield - LastShield;
	LastShield = Shield;

	OnShieldChanged.Broadcast(Shield, Delta);
}

void UMsHealthComponent::ResetHealth()
{
	if (const AActor* Owner = GetOwner())
	{
		if (!Owner->HasAuthority())
		{
			return;
		}
	}

	Health = MaxHealth;
	LastHealth = Health;

	Shield = MaxShield;
	LastShield = Shield;

	OnHealthChanged.Broadcast(Health, 0.0f);
	OnShieldChanged.Broadcast(Shield, 0.0f);
}
