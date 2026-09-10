/**
 * @file     : CoverageMaterial.qml
 * @brief    : Defines the material used to display inspection coverage.
 * @details  : Connects coverage texture data and the coverage shader.
 * @author   : Abhinay Chauhan (email: chauhan089306@gmail.com)
 * @version  : 1.0.0
 *
 * Copyright (c) 2024
 * All rights reserved.
 */

import QtQuick
import QtQuick3D
import AeroTwinGCS

/*!
    Inspection-coverage material for a CAD / glTF target asset.

        Model {
            source: "meshes/bridge_truss_a.mesh"
            materials: CoverageMaterial { id: skin }
        }

        // drive it from the coverage solver
        skin.coverage.splat(u, v, 0.02, quality)

    The asset needs a sane UV0 layout (a lightmap-style unwrap is ideal). The
    grid overlay is world-projected, so it still reads correctly on meshes with
    stretched or tiled UVs.
*/
CustomMaterial {
    id: material

    /** Uniforms injected into coverage.frag to define inspection-state appearance. */
    property real coverageThreshold: 0.85   // fully inspected at/above this quality
    property real partialThreshold: 0.25    // partially inspected at/above this quality
    property real glowStrength: 1.6
    property real rimPower: 2.2
    property real rimOpacity: 0.75
    property real rimGlow: 0.5
    property real gridScale: 0.02
    property real gridWidth: 0.03
    property real gridStrength: 0.35
    property real scanProgress: -1.0        // negative parks the sweep off-screen
    property real sweepWidth: 0.06
    property real sweepStrength: 1.2

    // R8 map accumulates the best camera quality measured for each asset UV texel.
    property TextureInput coverageMap: TextureInput {
        texture: Texture {
            textureData: CoverageMapTextureData {
                id: coverageData
                resolution: 512
                fullThreshold: material.coverageThreshold
            }
            minFilter: Texture.Linear
            magFilter: Texture.Linear
            tilingModeHorizontal: Texture.ClampToEdge
            tilingModeVertical: Texture.ClampToEdge
        }
    }

    // RGBA16F ramp converts normalized inspection state into linear shader colors.
    property TextureInput inspectionColorMap: TextureInput {
        texture: Texture {
            textureData: InspectionColorMap {
                id: colorRamp
                uninspectedColor: "#ff3b30"
                partialColor: "#ffcc00"
                inspectedColor: "#30d158"
                uninspectedOpacity: 0.16
                partialOpacity: 0.55
                inspectedOpacity: 0.92
            }
            minFilter: Texture.Linear
            magFilter: Texture.Linear
            tilingModeHorizontal: Texture.ClampToEdge
            tilingModeVertical: Texture.ClampToEdge
        }
    }

    /** Exposes texture data objects so a coverage solver can update the material. */
    readonly property CoverageMapTextureData coverage: coverageData
    readonly property InspectionColorMap palette: colorRamp

    // Alpha blending preserves the uninspected translucent shell over the asset.
    shadingMode: CustomMaterial.Shaded
    fragmentShader: "shaders/coverage.frag"
    sourceBlend: CustomMaterial.SrcAlpha
    destinationBlend: CustomMaterial.OneMinusSrcAlpha
    cullMode: Material.BackFaceCulling
    depthDrawMode: Material.NeverDepthDraw
}
