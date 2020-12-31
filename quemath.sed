s/Clamp\(/clampf\(/g
s/Min\(/minf\(/g
s/Max\(/maxf\(/g
s/Radians\(/radians(/g
s/Degrees\(/degrees(/g
s/SmoothStep/smoothf/g
s/VectorClear\((.+)\)/\1 = vec3_zero()/g
s/Vector4Clear\((.+)\)/\1 = vec4_zero()/g
s/VectorSet\((.+), (.+), (.+), (.+)\)/\1 = vec3f(\2, \3, \4)/g
s/Vector2Set\((.+), (.+), (.+)\)/\1 = vec2f(\2, \3)/g
s/Vector4Set\((.+), (.+), (.+), (.+), (.+)\)/\1 = vec4f(\2, \3, \4, \5)/g
s/VectorCopy\((.+), (.+)\)/\2 = \1/g
s/VectorAdd\((.+), (.+), (.+)\)/\3 = vec3_add(\1, \2)/g
s/VectorSubtract\((.+), (.+), (.+)\)/\3 = vec3_subtract(\1, \2)/g
s/VectorScale\((.+), (.+), (.+)\)/\3 = vec3_scale(\1, \2)/g
s/VectorMA\((.+), (.+), (.+), (.+)\)/\4 = vec3_add(\1, vec3_scale(\3, \2)/g
s/VectorMix\((.+), (.+), (.+), (.+)\)/\4 = vec3_mix(\1, \2, \3)/g
s/VectorLerp\((.+), (.+), (.+), (.+)\)/\4 = vec3_mix(\1, \2, \3)/g
s/Vector4Lerp\((.+), (.+), (.+), (.+)\)/\4 = vec4_mix(\1, \2, \3)/g
s/VectorLengthSquared/vec3_length_squared/g
s/VectorLength/vec3_length/g
s/VectorDistanceSquared/vec3_distance_squared/g
s/VectorDistance/vec3_distance/g
s/VectorNormalize\((.+)\)/\1 = vec3_normalize(\1)/g
s/VectorNormalize2\((.+), (.+)\)/\2 = vec3_normalize(\1)/g
s/VectorAngles\((.+), (.+)\)/\2 = vec3_degrees(vec3_euler(\1))/g
s/AngleVectors\((.+), (.+), (.+), (.+)\)/vec3_vectors(\1, \2, \3, \4)/g
s/VectorCompare/vec3_equal/g
s/Vector2Compare/vec2_equal/g
s/Vector4Compare/vec4_equal/g
s/CrossProduct\((.+), (.+), (.+)\)/\3 = vec3_cross(\1, \2)/g
s/DotProduct/vec3_dot/g
s/Reflect\((.+), (.+), (.+)\)/\3 = vec3_reflect(\1, \2)/g
s/TangentVectors\((.+), (.+), (.+), (.+), (.+)\)/vec3_tangents(\1, \2, \3, &\4, &\5)/g
s/AnglesLerp/vec3_mix_euler/g
s/VectorNegate\((.+), (.+)\)/\2 = vec3_negate(\1)/g
s/ColorDecomponse3\((.+_), (.+)\)/\2 = ColorDecompose3(\1)/g
s/ColorDecomponse\((.+_), (.+)\)/\2 = ColorDecompose(\1)/g
s/ColorFilter\((.+), (.+), (.+), (.+), (.+)\)/\2 = ColorFilter(\1, \3, \4, \5)/g

