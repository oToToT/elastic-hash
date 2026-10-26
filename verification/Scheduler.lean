import Std

/-!
A dependency-free Lean 4 proof of the dyadic scheduler's deterministic rank
bound. This is NOT a proof of random-hash independence, the paper's probabilistic
batch lemmas, or refinement of the complete C++ implementation.

In epoch r, level i in band k=floor(log2 i) has a cumulative allowance of
2^floor((r-k)/2) probes. Band k has 2^k levels. Advancing by two epochs doubles
all old allowances and adds bands r+1 and r+2 with allowance one. Their total
size is 3*2^(r+1). `epochWork` counts the resulting cumulative allowances.
-/
namespace ElasticHash

def epochWork : Nat → Nat
  | 0 => 1
  | 1 => 3
  | r + 2 => 2 * epochWork r + 3 * 2^(r+1)

-- An explicit sum of the allowances over the first `bands` level bands.
def bandTotal (r : Nat) : Nat → Nat
  | 0 => 0
  | bands + 1 => bandTotal r bands + 2^bands * 2^((r-bands)/2)

theorem old_bands_double (r bands : Nat) (within : bands ≤ r+1) :
    bandTotal (r+2) bands = 2 * bandTotal r bands := by
  induction bands with
  | zero => simp [bandTotal]
  | succ bands ih =>
    have previous := ih (by omega)
    have exponent : (r+2-bands)/2 = (r-bands)/2+1 := by omega
    simp only [bandTotal, previous, exponent, Nat.pow_succ, ← Nat.mul_assoc]
    omega

-- Thus the recurrence above counts the ACTUAL cumulative allowance, rather
-- than introducing its claimed asymptotic bound as an assumption.
theorem epoch_work_counts_bands (r : Nat) :
    epochWork r = bandTotal r (r+1) := by
  induction r using Nat.strongRecOn with
  | ind r ih =>
    cases r with
    | zero => decide
    | succ r =>
      cases r with
      | zero => decide
      | succ r =>
        have previous := ih r (by omega)
        have doubled := old_bands_double r (r+1) (by omega)
        have e0 : r+2-(r+2) = 0 := by omega
        have e1 : r+2-(r+1) = 1 := by omega
        change 2 * epochWork r + 3 * 2^(r+1) =
          bandTotal (r+2) (r+1) + 2^(r+1) * 2^((r+2-(r+1))/2) +
            2^(r+2) * 2^((r+2-(r+2))/2)
        rw [previous, doubled, e0, e1]
        simp only [Nat.pow_succ]
        omega

theorem epoch_work_bound (r : Nat) : epochWork r ≤ 4 * 2^r := by
  induction r using Nat.strongRecOn with
  | ind r ih =>
    cases r with
    | zero => decide
    | succ r =>
      cases r with
      | zero => decide
      | succ r =>
        have prev := ih r (by omega)
        simp only [epochWork, Nat.pow_succ]
        omega

-- A target (i,j) is eligible by epoch k+2*q when i's band is k and
-- q=ceil(log2 j). The lower band boundary and upper rounded probe count
-- are all that the cost inequality needs.
theorem rank_bound (i j k q : Nat)
    (band_lower : 2^k ≤ i) (probe_upper : 2^q ≤ 2*j) :
    epochWork (k + 2*q) ≤ 16*i*j*j := by
  have h := epoch_work_bound (k + 2*q)
  have hp := Nat.mul_le_mul probe_upper probe_upper
  have hb := Nat.mul_le_mul band_lower hp
  have hc := Nat.mul_le_mul_left 4 hb
  have power : 2^(k+2*q) = 2^k * (2^q * 2^q) := by
    rw [show 2*q = q+q by omega, Nat.pow_add, Nat.pow_add]
  rw [power] at h
  have normalized : 4 * (i * (2*j*(2*j))) = 16*i*j*j := by
    simp [Nat.mul_comm, Nat.mul_left_comm]
  rw [normalized] at hc
  exact Nat.le_trans h hc

-- Concrete witnesses for every positive probe pair; no rounding hypotheses
-- are needed by callers of these two theorems. Using floor(log2 j)+1 may
-- select one epoch pair later than necessary for powers of two, which is safe.
def targetEpoch (i j : Nat) : Nat := i.log2 + 2*(j.log2+1)

theorem target_is_scheduled (i j : Nat) :
    j ≤ 2^((targetEpoch i j - i.log2)/2) := by
  have exponent : (targetEpoch i j - i.log2)/2 = j.log2+1 := by
    unfold targetEpoch
    omega
  rw [exponent]
  exact Nat.le_of_lt Nat.lt_log2_self

theorem target_rank_bound (i j : Nat) (hi : 0 < i) (hj : 0 < j) :
    epochWork (targetEpoch i j) ≤ 16*i*j*j := by
  have upper := Nat.mul_le_mul_right 2 (Nat.log2_self_le (by omega : j ≠ 0))
  apply rank_bound
  · exact Nat.log2_self_le (by omega)
  · simpa [Nat.pow_succ, Nat.mul_comm] using upper

-- A finite weighted model makes the exact remaining probabilistic obligation
-- explicit. Weights can represent frequencies or rational probabilities with
-- a common denominator. No random-hash assumption is smuggled in as an axiom.
structure Sample where
  level : Nat
  probe : Nat
  band : Nat
  roundedProbeExponent : Nat
  weight : Nat

def Valid (s : Sample) : Prop :=
  2^s.band ≤ s.level ∧ s.level < 2^(s.band+1) ∧
  s.probe ≤ 2^s.roundedProbeExponent ∧ 2^s.roundedProbeExponent ≤ 2*s.probe

def weightedCost : List Sample → Nat
  | [] => 0
  | s :: rest => s.weight * epochWork (s.band + 2*s.roundedProbeExponent) +
                  weightedCost rest

def weightedMoment : List Sample → Nat
  | [] => 0
  | s :: rest => s.weight * (s.level*s.probe*s.probe) + weightedMoment rest

theorem weighted_cost_bound (samples : List Sample)
    (valid : ∀ s ∈ samples, Valid s) :
    weightedCost samples ≤ 16 * weightedMoment samples := by
  induction samples with
  | nil => simp [weightedCost, weightedMoment]
  | cons s rest ih =>
    have hs := valid s (by simp)
    have hr := ih (by intro t ht; exact valid t (by simp [ht]))
    have bound := rank_bound s.level s.probe s.band s.roundedProbeExponent hs.1 hs.2.2.2
    have scaled := Nat.mul_le_mul_left s.weight bound
    have reorder : s.weight * (16*s.level*s.probe*s.probe) =
        16 * (s.weight * (s.level*s.probe*s.probe)) := by
      simp [Nat.mul_comm, Nat.mul_left_comm, Nat.mul_assoc]
    rw [reorder] at scaled
    simp only [weightedCost, weightedMoment]
    omega

-- If a batch analysis supplies E[i*j²] <= C*L, the schedule costs <=16*C*L
-- in expectation. L is 1 for the amortized bound and O(log(1/delta)) for
-- the per-key bound. The hypothesis is a proof obligation, not a conclusion
-- about the C++ mixer or the engineering extensions.
theorem conditional_complexity_transfer (samples : List Sample)
    (valid : ∀ s ∈ samples, Valid s) (C L mass : Nat)
    (paper_moment_bound : weightedMoment samples ≤ C*L*mass) :
    weightedCost samples ≤ 16*(C*L*mass) := by
  exact Nat.le_trans (weighted_cost_bound samples valid)
    (Nat.mul_le_mul_left 16 paper_moment_bound)

-- Finite probe limits: the fallback visits each slot exactly once. The
-- following injectivity lemma is independent of any quality of the hash.
theorem fallback_injective (capacity hash a b : Nat)
    (positive : 0 < capacity) (ha : a < capacity) (hb : b < capacity)
    (same : (hash + a) % capacity = (hash + b) % capacity) : a = b := by
  have hmod := Nat.mod_lt hash positive
  have reduce (x : Nat) (hx : x < capacity) :
      (hash + x) % capacity =
        if hash % capacity + x < capacity then hash % capacity + x
        else hash % capacity + x - capacity := by
    rw [Nat.add_mod, Nat.mod_eq_of_lt hx]
    split
    next h => exact Nat.mod_eq_of_lt h
    next h =>
      rw [Nat.mod_eq_sub_mod (by omega)]
      exact Nat.mod_eq_of_lt (by omega)
  rw [reduce a ha, reduce b hb] at same
  split at same <;> split at same <;> omega

#print axioms epoch_work_counts_bands
#print axioms epoch_work_bound
#print axioms rank_bound
#print axioms target_is_scheduled
#print axioms target_rank_bound
#print axioms fallback_injective
#print axioms weighted_cost_bound
#print axioms conditional_complexity_transfer
end ElasticHash
