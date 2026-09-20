/*
 * Copyright(c) 1997-2001 id Software, Inc.
 * Copyright(c) 2002 The Quakeforge Project.
 * Copyright(c) 2006 Quetoo.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

/**
 * @file light.glsl
 * @brief Implements shared vertex and fragment lighting helpers for lit materials.
 * @remarks Include after common.glsl, material.glsl, and voxel.glsl.
 */

#include "light_types.glsl"

/**
 * @brief Blends full fragment lighting down to vertex lighting beyond lightingDistance.
 */
#define LIGHTING_LOD_BLEND_DIST 128.0

#if defined(FRAGMENT_SHADER)
/**
 * @brief 2D Poisson disk samples for PCF soft shadows.
 */
const vec2 poissonDisk[16] = vec2[](
  vec2( 0.2770745,  0.6951455),
  vec2(-0.5932785, -0.1203284),
  vec2( 0.4494750,  0.2469098),
  vec2(-0.1460639, -0.5679667),
  vec2( 0.6400498, -0.4071948),
  vec2(-0.3631914,  0.7935778),
  vec2( 0.1248857, -0.8975238),
  vec2(-0.7720318,  0.4438458),
  vec2( 0.8851806,  0.1653373),
  vec2(-0.5238012, -0.7260296),
  vec2( 0.3642682,  0.5968054),
  vec2(-0.8331701, -0.3328346),
  vec2( 0.5527260, -0.6985809),
  vec2(-0.2407123,  0.3153157),
  vec2( 0.7269405, -0.1430640),
  vec2(-0.6444675,  0.6444675)
);

/**
 * @brief Simple pseudo-random function for per-pixel rotation.
 */
float randomAngle(vec3 seed) {
  return fract(sin(dot(seed, vec3(12.9898, 78.233, 45.164))) * 43758.5453) * 6.283185;
}

/**
 * @brief Determine cubemap face index and compute face UV from direction vector.
 * @brief Computes the cubemap face and face UV for a light-to-fragment direction.
 */
void cubemapFaceUv(in vec3 dir, out int face, out vec2 faceUv, out float ma) {

  vec3 ad = abs(dir);
  float sc, tc;

  if (ad.x >= ad.y && ad.x >= ad.z) {
    ma = ad.x;
    if (dir.x > 0.0) {
      face = 0; sc = -dir.z; tc = -dir.y;
    } else {
      face = 1; sc = dir.z; tc = -dir.y;
    }
  } else if (ad.y >= ad.x && ad.y >= ad.z) {
    ma = ad.y;
    if (dir.y > 0.0) {
      face = 2; sc = dir.x; tc = dir.z;
    } else {
      face = 3; sc = dir.x; tc = -dir.z;
    }
  } else {
    ma = ad.z;
    if (dir.z > 0.0) {
      face = 4; sc = dir.x; tc = -dir.y;
    } else {
      face = 5; sc = -dir.x; tc = -dir.y;
    }
  }

  faceUv = vec2(sc, tc) / (2.0 * ma) + 0.5;
}

/**
 * @brief Samples the shadow atlas face texture selected by face.
 */
float sampleShadowFace(in int face, in vec3 uvw) {
  if (face == 0) {
    return texture(textureShadowAtlas0, uvw);
  } else if (face == 1) {
    return texture(textureShadowAtlas1, uvw);
  } else if (face == 2) {
    return texture(textureShadowAtlas2, uvw);
  } else if (face == 3) {
    return texture(textureShadowAtlas3, uvw);
  } else if (face == 4) {
    return texture(textureShadowAtlas4, uvw);
  } else {
    return texture(textureShadowAtlas5, uvw);
  }
}

/**
 * @brief Samples the shadow atlas for a light with PCF filtering.
 */
float sampleShadowAtlas(in Light light, in CommonVertex v, in CommonFragment f, in float atten) {

  if (light.tile.x < 0.0) {
    return 1.0;
  }

  vec2 texSize = vec2(textureSize(textureShadowAtlas0, 0).xy);
  float tilePx = texSize.x / float(SHADOW_ATLAS_LIGHTS_PER_ROW);
  vec2 tileOrigin = light.tile.xy / texSize;
  float tileUv = tilePx / texSize.x;

  vec3 lightToFrag = v.modelPosition - light.origin.xyz;
  float distToLight = length(lightToFrag);

  float lightSize = light.origin.w * 3.0;
  float filterRadius = lightSize * (distToLight / light.origin.w) * 0.005;

  // Offset the receiver along its normal to combat acne. Every PCF tap
  // below is compared against a single reference depth computed here, so
  // the offset must cover the worst-case depth variation across the
  // *entire* filter footprint (filterRadius), not just one texel --
  // otherwise grazing/curved surfaces alias against their neighboring taps.
  // Note: v.normal is view-space; v.modelNormal is the world/model-space
  // normal that actually matches v.modelPosition and light.origin here.
  float nDotL = max(dot(v.modelNormal, normalize(-lightToFrag)), 0.0);
  vec3 offsetPosition = v.modelPosition + v.modelNormal * filterRadius * (1.0 - nDotL);

  lightToFrag = offsetPosition - light.origin.xyz;
  distToLight = length(lightToFrag);
  float currentDepth = distToLight / light.origin.w;

  int face;
  vec2 fuv;
  float ma;
  cubemapFaceUv(lightToFrag, face, fuv, ma);

  fuv.y = 1.0 - fuv.y;

  vec2 halfTexel = 0.5 / texSize;
  vec2 tileMin = tileOrigin + halfTexel;
  vec2 tileMax = tileOrigin + vec2(tileUv) - halfTexel;

  float filterUv = filterRadius / (2.0 * max(ma, 0.001));

  float importance = atten * clamp(1.0 - f.viewDist / 2048.0, 0.0, 1.0);
  int numSamples = importance > 0.3 ? 8 : (importance > 0.1 ? 4 : 2);

  float s = f.shadowSinCos.x;
  float c = f.shadowSinCos.y;

  float shadow = 0.0;

  for (int i = 0; i < numSamples; i++) {
    vec2 rotated = vec2(c * poissonDisk[i].x - s * poissonDisk[i].y,
                        s * poissonDisk[i].x + c * poissonDisk[i].y);

    vec2 sampleFuv = fuv + rotated * filterUv;

    vec2 atlasUv = tileOrigin + sampleFuv * vec2(tileUv);

    atlasUv = clamp(atlasUv, tileMin, tileMax);

    shadow += sampleShadowFace(face, vec3(atlasUv, currentDepth));
  }

  return shadow / float(numSamples);
}

/**
 * @brief Evaluates the Blinn specular term.
 */
float blinn(in vec3 lightDir, in CommonFragment f) {
  return pow(max(0.0, dot(normalize(lightDir + f.viewDir), f.normalSample)), f.specularSample.w);
}

/**
 * @brief Evaluates the Blinn-Phong specular contribution for a light.
 */
vec3 blinnPhong(in vec3 lightColor, in vec3 lightDir, in CommonFragment f) {
  return lightColor * f.specularSample.rgb * blinn(lightDir, f);
}
#endif

/**
 * @brief Computes ambient lighting from the sky cubemap and voxel data.
 */
vec3 ambientLight(in CommonVertex v) {

  float occlusion = voxelOcclusion(v.voxel);
  float exposure = voxelExposure(v.voxel);

  vec3 sky = textureLod(textureSky, normalize(v.modelNormal), 6).rgb;
  return pow(vec3(2.0) + sky, vec3(2.0)) * exposure * (1.0 - occlusion * ambientOcclusion) * ambient;
}

/**
 * @brief Computes unshadowed diffuse vertex lighting from one light.
 */
vec3 vertexLight(in CommonVertex v, in Light light) {

  vec3 lightDir = light.origin.xyz - v.modelPosition;
  float dist = length(lightDir);
  float radius = light.origin.w;
  float atten = clamp(1.0 - dist / radius, 0.0, 1.0);

  if (atten <= 0.0) {
    return vec3(0.0);
  }

  lightDir = normalize(lightDir);
  float lambert = dot(v.modelNormal, lightDir);
  lambert = bool(material.surface & (SURF_MASK_BLEND | SURF_LIQUID)) ? abs(lambert) : max(0.0, lambert);
  return lightColor(light) * atten * lambert;
}

/**
 * @brief Caches the vertex caustics strength.
 */
void vertexCaustics(inout CommonVertex v) {
  v.caustics = length(voxelCaustics(v.voxel));
}

/**
 * @brief Accumulates the vertex lighting fallback for a draw.
 */
void vertexLighting(inout CommonVertex v) {

  v.ambient = ambientLight(v);
  v.diffuse = vec3(0.0);

  if (editor == 0) {
    ivec3 voxelCoord = voxelXyz(v.modelPosition);
    ivec2 data = voxelLightData(voxelCoord);

    for (int i = 0; i < data.y; i++) {
      int index = voxelLightIndex(data.x + i);
      v.diffuse += vertexLight(v, bspLights[index]);
    }
  }

  for (int j = 0; j < numDynamicLights; j++) {
    if (dynamicLightActive(activeDynamicLights, j)) {
      v.diffuse += vertexLight(v, dynamicLights[j]);
    }
  }

  vertexCaustics(v);
}

#if defined(FRAGMENT_SHADER)
/**
 * @brief Applies animated caustics to the fragment diffuse lighting.
 */
void fragmentCaustics(in CommonVertex v, inout CommonFragment f) {

  vec3 causticsSample = voxelCaustics(v.voxel);

  float causticsStrength = length(causticsSample);
  if (causticsStrength == 0.0) {
    return;
  }

  vec3 causticsDir = normalize(mat3(view) * causticsSample);
  float facing = dot(v.normal, causticsDir);
  float backface = facing < -0.25 ? 0.25 : 1.0;
  f.caustics = causticsStrength * backface;

  if (f.caustics == 0.0) {
    return;
  }

  float noise = noise3d(v.modelPosition * .05 + (ticks / 1000.0) * 0.5);

  float thickness = 0.02;
  float glow = 5.0;

  noise = clamp(pow((1.0 - abs(noise)) + thickness, glow), 0.0, 1.0);

  vec3 light = f.ambient + f.diffuse;
  f.diffuse += max(vec3(0.0), light * f.caustics * noise);
}

/**
 * @brief Raymarches parallax self-shadowing along the light direction.
 */
#if defined(PARALLAX_SELF_SHADOW)
float parallaxSelfShadow(in vec3 lightDir, in CommonVertex v, in CommonFragment f) {

  int maxSteps = int(mix(12.0, 2.0, min(f.texLod * 0.5, 1.0)));

  float stepScale = mix(1.0, 4.0, min(f.texLod * 0.5, 1.0));

  vec2 texel = 1.0 / textureSize(textureMaterial, 0).xy;
  vec3 dir = normalize(vec3(dot(lightDir, v.tangent), dot(lightDir, v.bitangent), dot(lightDir, v.normal)));
  vec3 delta = vec3(dir.xy * texel, max(dir.z * length(texel), .01)) * stepScale;
  vec3 texcoord = vec3(f.parallax, sampleMaterialHeightmap(f.parallax, f.texLod));

  float maxHeight = texcoord.z;
  for (int i = 0; i < maxSteps && texcoord.z < 1.0 && maxHeight < 1.0; i++) {
    texcoord += delta;
    maxHeight = max(maxHeight, sampleMaterialHeightmap(texcoord.xy, f.texLod));
  }

  float shadow = 1.0 - (maxHeight - texcoord.z) * material.shadow;
  return clamp(shadow, 0.0, 1.0);
}
#endif

/**
 * @brief Accumulates diffuse, specular, and shadowing from one light.
 */
void fragmentLight(in CommonVertex v, inout CommonFragment f, in Light light) {

  vec3 dir = light.origin.xyz - v.modelPosition;
  float dist = length(dir);
  float radius = light.origin.w;
  float atten = clamp(1.0 - dist / radius, 0.0, 1.0);
  if (atten <= 0.0) {
    return;
  }

  dir = normalize(view * vec4(dir, 0.0)).xyz;

  bool isBlend = bool(material.surface & SURF_MASK_BLEND);
  bool isLiquid = bool(material.surface & SURF_LIQUID);
  bool isStage = bool(material.flags != STAGE_NONE);

  float lambert = dot(dir, f.normalSample);
  lambert = isBlend || isLiquid || isStage ? abs(lambert) : max(0.0, lambert);

  if (atten * lambert <= 0.0) {
    return;
  }

  vec3 color = lightColor(light) * atten;

  float shadow = sampleShadowAtlas(light, v, f, atten);

#if defined(PARALLAX_SELF_SHADOW)
  if (!isStage && material.shadow > 0.0 && f.texLod < 2.0) {
    shadow *= parallaxSelfShadow(dir, v, f);
  }
#endif

  if (shadow <= 0.0) {
    return;
  }

  f.diffuse += color * lambert * shadow;
  f.specular += blinnPhong(color * shadow, dir, f);
}

/**
 * @brief Accumulates full fragment lighting for the active BSP and dynamic lights.
 */
void fragmentLighting(in CommonVertex v, inout CommonFragment f) {

  f.ambient = ambientLight(v);
  f.diffuse = vec3(0.0);
  f.specular = vec3(0.0);

  if (editor == 0) {
    ivec3 voxelCoord = voxelXyz(v.modelPosition);
    ivec2 data = voxelLightData(voxelCoord);

    for (int i = 0; i < data.y; i++) {
      int index = voxelLightIndex(data.x + i);
      fragmentLight(v, f, bspLights[index]);
    }
  }

  for (int j = 0; j < numDynamicLights; j++) {
    if (dynamicLightActive(activeDynamicLights, j)) {
      fragmentLight(v, f, dynamicLights[j]);
    }
  }

  fragmentCaustics(v, f);
}

/**
 * @brief Computes full fragment lighting, blending down to vertex lighting as
 * fragment.viewDist approaches lightingDistance.
 */
void fragmentLightingLod(in CommonVertex v, inout CommonFragment f) {

  const float lightingLod = clamp((f.viewDist - lightingDistance) / LIGHTING_LOD_BLEND_DIST, 0.0, 1.0);

  if (lightingLod >= 1.0) {
    f.ambient = v.ambient;
    f.diffuse = v.diffuse;
    f.specular = vec3(0.0);
    return;
  }

  if ((material.flags & STAGE_LIGHTING_FLAT) == STAGE_LIGHTING_FLAT) {
    f.normalSample = normalize(v.normal);
    f.specularSample = vec4(f.diffuseSample.rgb, pow(1.0 + material.specularity, 4.0));
  } else {
    f.normalSample = sampleMaterialNormal(f.parallax, mat3(v.tangent, v.bitangent, v.normal));
    f.specularSample = sampleMaterialSpecular(f.parallax);
  }

  float angle = randomAngle(v.modelPosition);
  f.shadowSinCos = vec2(sin(angle), cos(angle));

  fragmentLighting(v, f);

  f.ambient = mix(f.ambient, v.ambient, lightingLod);
  f.diffuse = mix(f.diffuse, v.diffuse, lightingLod);
  f.specular *= 1.0 - lightingLod;
}
#endif
