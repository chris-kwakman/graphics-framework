#version 420 core

#extension GL_ARB_explicit_uniform_location : enable

layout(location = 0) uniform mat4 u_v;
layout(location = 1) uniform mat4 u_p;

out vec3 near_point;
out vec3 far_point;
out mat4 f_v;
out mat4 f_p;

// Grid position are in clipped space
vec3 gridPlane[6] = vec3[] (
    vec3(-1, -1, 0), vec3(1, 1, 0), vec3(-1, 1, 0),
    vec3(1, 1, 0), vec3(-1, -1, 0), vec3(1, -1, 0)
);

vec3 UnprojectPoint(float x, float y, float z, mat4 view, mat4 projection) {
    mat4 viewInv = inverse(view);
    mat4 projInv = inverse(projection);
    vec4 unprojectedPoint =  viewInv * projInv * vec4(x, y, z, 1.0);
    return unprojectedPoint.xyz / unprojectedPoint.w;
}

void main() {
    vec3 p = gridPlane[gl_VertexID].xyz;
    near_point = UnprojectPoint(p.x, p.y, 0.0, u_v, u_p).xyz; // unprojecting on the near plane
    far_point = UnprojectPoint(p.x, p.y, 1.0, u_v, u_p).xyz; // unprojecting on the far plane
    gl_Position = vec4(p, 1.0); // using directly the clipped coordinates

    f_v = u_v;
    f_p = u_p;
}