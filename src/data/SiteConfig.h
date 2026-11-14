#pragma once

// M1a: single source of truth for the site constants and the four prepared
// layer descriptors (AGENTS.md §9: hard-coded coordinates/thresholds are
// named and commented here). The registry and the tests both build from
// these descriptors so the user-facing strings cannot drift apart.

#include "data/RasterLayer.h"

#include <QString>

#include <vector>

namespace SiteConfig
{
// --- Fixed site facts (do not change; see brief / PLAN.md) ---------------
// Mars mean radius, metres (IAU Mars 2015 body).
constexpr double kMarsRadiusM = 3396190.0;
// Site equirectangular central meridian, degrees east.
constexpr double kCentralMeridianDeg = 137.39;
// Site CRS PROJ string. Equals PlanetaryCrs::marsEquirectangular(137.39).
constexpr const char *kSiteCrsProj4 =
    "+proj=eqc +lat_ts=0 +lon_0=137.39 +R=3396190 +units=m +no_defs";
// Human-readable site name shown in the UI.
constexpr const char *kSiteName =
    "Gale Crater \xE2\x80\x94 Pahrump Hills / Garden City";
// Mission anchor point (lat/lon degrees) — used for walkback/comm checks.
constexpr double kAnchorLatDeg = -4.695;
constexpr double kAnchorLonDeg = 137.381;
// Site bounding box in degrees (west/east/south/north).
constexpr double kBBoxWestDeg = 137.33518211;
constexpr double kBBoxEastDeg = 137.44270087;
constexpr double kBBoxSouthDeg = -4.792202743651;
constexpr double kBBoxNorthDeg = -4.583317928209;
// Layer shown when nothing else is selected.
constexpr const char *kDefaultLayerId = "hirise_optical";

// --- Asset location -----------------------------------------------------
// Non-empty CETUS_ASSET_DIR env var wins; else the CETUS_ASSET_DIR compile
// macro (set by CMake to <source>/assets); else empty string — callers must
// fail loudly on an empty asset dir. Never falls back to the CWD.
QString assetDir();

// Full path to a prepared asset file: <assetDir>/layers/<fileName> (the
// prepared rasters ship in assets/layers/ while CETUS_ASSET_DIR points at
// assets/). Returns an empty string when the asset directory is unknown
// (callers must fail loudly).
QString layerPath(const QString &fileName);

// Full path to the M3 scenario JSON: <assetDir>/scenario/m3_scenario.json.
// Returns an empty string when the asset directory is unknown (callers must
// fail loudly).
QString scenarioPath();

// --- Prepared layer descriptors (one source for registry and tests) -----
// Display order used by the registry: hillshade, dem, slope, optical.
std::vector<RasterLayer::Metadata> preparedLayerMetadata();

RasterLayer::Metadata hillshadeLayerMetadata();
RasterLayer::Metadata demLayerMetadata();
RasterLayer::Metadata slopeLayerMetadata();
RasterLayer::Metadata opticalLayerMetadata();
}  // namespace SiteConfig
