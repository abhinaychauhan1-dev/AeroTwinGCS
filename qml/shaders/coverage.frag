/**
 * @file     : coverage.frag
 * @brief    : Shades the visual inspection coverage surface.
 * @details  : Colors inspected, partial, and uninspected asset regions.
 * @author   : Abhinay Chauhan (email: chauhan089306@gmail.com)
 * @version  : 1.0.0
 *
 * Copyright (c) 2024
 * All rights reserved.
 */

// Inspection coverage visualisation.
//
// Qt Quick 3D CustomMaterial fragment shader (shadingMode: Shaded).
// Written against the Qt Shader Tools dialect, so it is transpiled by qsb into
// SPIR-V / DXBC / MSL / GLSL and runs unmodified on Vulkan, D3D12, Metal and
// OpenGL. Do not add layout qualifiers or samplers by hand: every QML property
// on the CustomMaterial is injected as a uniform of the same name, and every
// TextureInput property as a sampler of the same name.
//
// Inputs (from CoverageMaterial.qml):
//   coverageMap         R8    accumulated inspection quality in asset UV space
//   inspectionColorMap  ramp  uninspected -> partial -> inspected, alpha = opacity
//   partialThreshold    float coverage at which a surface counts as partial
//   coverageThreshold   float coverage at which a surface counts as complete

void MAIN()
{
    // Sample per-texel inspection quality written by CoverageMapTextureData.
    float coverage = clamp(texture(coverageMap, UV0).r, 0.0, 1.0);

    float partialCut = clamp(partialThreshold, 0.0, 0.98);
    float fullCut = clamp(coverageThreshold, partialCut + 0.01, 1.0);

    // Normalise raw quality into the ramp's 0 / 0.5 / 1 state space.
    float state = coverage < partialCut
        ? 0.5 * smoothstep(0.0, partialCut, coverage)
        : 0.5 + 0.5 * smoothstep(partialCut, fullCut, coverage);
    state = clamp(state, 0.0, 1.0);

    // The 1D color ramp supplies linear RGB and state-dependent transparency.
    vec4 ramp = texture(inspectionColorMap, vec2(state, 0.5));
    vec3 tint = ramp.rgb;
    float alpha = ramp.a;

    // View-facing geometry contributes the edge glow used to highlight missed areas.
    vec3 normal = normalize(NORMAL);
    vec3 view = normalize(VIEW_VECTOR);
    float facing = clamp(abs(dot(normal, view)), 0.0, 1.0);

    // Rim term: gives uninspected geometry a translucent shell with a hot
    // silhouette instead of a flat wash of colour.
    float rim = pow(1.0 - facing, max(rimPower, 0.001));

    // Triplanar-lite inspection grid, projected on the dominant world axis so
    // it survives non-uniform UV layouts on imported CAD/glTF meshes.
    vec3 gridPos = VAR_WORLD_POSITION * max(gridScale, 0.0001);
    vec3 axis = abs(normal);
    vec2 gridUv = (axis.y > max(axis.x, axis.z))
        ? gridPos.xz
        : ((axis.x > axis.z) ? gridPos.zy : gridPos.xy);
    vec2 cell = abs(fract(gridUv) - 0.5);
    float gridLine = 1.0 - smoothstep(0.0, gridWidth, min(cell.x, cell.y));

    // Sweep pulse riding the leading edge of the coverage front.
    float sweep = smoothstep(sweepWidth, 0.0, abs(coverage - scanProgress));

    // Uninspected surfaces stay see-through in the middle and glow at the edge.
    float shellAlpha = clamp(alpha + rim * rimOpacity * (1.0 - state), 0.0, 1.0);

    float grid = gridLine * gridStrength * state;
    vec3 emissive = tint * (glowStrength * state * state
                            + grid
                            + sweep * sweepStrength
                            + rim * rimGlow);

    // Output uses alpha for the coverage shell and emission for active scan visibility.
    BASE_COLOR = vec4(tint, shellAlpha);
    EMISSIVE_COLOR = emissive;
    METALNESS = 0.0;
    ROUGHNESS = mix(0.9, 0.25, state);
    SPECULAR_AMOUNT = mix(0.05, 0.5, state);
}
