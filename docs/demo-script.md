# Cetus — demo script

Target length: **2–3 minutes**. Everything runs offline from local assets.

## Setup

- Region: Mount Sharp / Gale Crater.
- Science goal: sample boxwork / fracture-filling vein deposits.
- Layers on: optical (HiRISE/CTX), topographic (DEM slope), spectral (CRISM
  hydrated/sulfate). Toggle thermal/radiation if ready.
- Mission state: Sol 3, mid-morning, tau nominal, consumables ~80%.

## Beats

1. **Orient (10 s).** Open the map. Show the region, the three vein targets
   (T1, T2, T3), and the crew position. Point at the layer legend: each layer
   names its source and resolution.

2. **Recommend (20 s).** Side panel shows the science goal, the ranked targets,
   and one primary recommendation: *T2, boxwork ridge, 140 m, ~12 min, sulfate +
   hydrated*. Read the "why": fluid chemistry, multiple flow episodes,
   biosignature preservation.

3. **Route (20 s).** Show the drawn traverse: 412 m, ~21 min, max slope 9°,
   boulder field avoided, safe haven marked, walkback margin OK. Note that the
   route is weighted by science value, not just distance.

4. **Suit reality (15 s).** Consumables (O2 / power / water) and suit speed are
   visible next to the route. Frame it as field equipment, not mission control.

5. **Change (30 s).** Raise dust opacity past the threshold. The tool drops the
   farthest, lowest-value target (T3), re-ranks, and reroutes to keep walkback
   margin. Warning banner explains *why*.

6. **Act (25 s).** Astronaut reaches a target and presses COLLECT SAMPLE. The
   tool assigns `SAMPLE-<sol>-<n>`, geotags the Mars Trek target pin, records
   the DEM up-vector and slope, and appends the auto-filled field note. State
   plainly that this is a field note: no physical sample and no composition
   measurement is claimed.

7. **Close (10 s).** One sentence on NASA data used and the safety model.

## What the judges should remember

- Real NASA data, three+ kinds, fused.
- Science goal → target ranking → safe route, in one loop.
- Safety and suit limits are first-class, not decoration.
- The tool reacts to changing conditions.
