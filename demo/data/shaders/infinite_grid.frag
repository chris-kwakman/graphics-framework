#version 420 core

#extension GL_ARB_explicit_uniform_location : enable

in mat4 f_v;
in mat4 f_p;

layout(location = 10) uniform float u_camera_near;
layout(location = 11) uniform float u_camera_far;

in vec3 near_point;
in vec3 far_point;

out vec4 outColor;

vec4 grid(vec3 fragPos3D, float scale, bool drawAxis) {
    vec2 coord = fragPos3D.xz * scale;
    vec2 derivative = fwidth(coord);
    vec2 grid = abs(fract(coord - 0.5) - 0.5) / derivative;
    float line = min(grid.x, grid.y);
    float minimumz = min(derivative.y, 1);
    float minimumx = min(derivative.x, 1);
    vec4 color = vec4(0.2, 0.2, 0.2, 1.0 - min(line, 1.0));
    // z axis
    if(fragPos3D.x > -0.1 * minimumx && fragPos3D.x < 0.1 * minimumx)
        color.z = 1.0;
    // x axis
    if(fragPos3D.z > -0.1 * minimumz && fragPos3D.z < 0.1 * minimumz)
        color.x = 1.0;
    return color;
}
float computeDepth(vec3 pos) {
    vec4 clip_space_pos = f_p * f_v * vec4(pos.xyz, 1.0);
   return ((clip_space_pos.z / clip_space_pos.w)+1)/2;
}
float computeLinearDepth(vec3 pos) {
    vec4 clip_space_pos = f_p * f_v* vec4(pos.xyz, 1.0);
    float clip_space_depth = (clip_space_pos.z / clip_space_pos.w) * 2.0 - 1.0; // put back between -1 and 1
    float linearDepth = (2.0 * u_camera_near * u_camera_far) / (u_camera_far + u_camera_near - clip_space_depth * (u_camera_far - u_camera_near)); // get linear value between 0.01 and 100
    return linearDepth / (u_camera_far - u_camera_near); // normalize
}
void main() {
    float t = -near_point.y / (far_point.y - near_point.y);
    vec3 fragPos3D = near_point + t * (far_point - near_point);

    gl_FragDepth = computeDepth(fragPos3D);
    
    float linearDepth = computeLinearDepth(fragPos3D);
    float fading = max(0, (0.5 - linearDepth));
    
    outColor = (grid(fragPos3D, 10, true) + grid(fragPos3D, 1, true))* float(t > 0); // adding multiple resolution for the grid
    outColor.a *= fading;

    if(fading > 1)
		discard;
	if(outColor.a < 0.01)
		discard;
}