// A particle emitter, start to finish: the compute pass that moves its particles and the two stages
// that draw them. One file because the three share the one thing that has to agree between them,
// which is the layout of a particle.
//
// Nothing here is ever read back. The particles are written by `CSSimulate`, read by `VSMain`, and
// seen by the processor not at all -- it sends a block of constants describing the emitter and a
// range of slots to refill, and that is the whole of its part.
static const float TWO_PI = 6.28318530718;
// The group's width, KUKI_PARTICLE_GROUP_SIZE, and the value of `ParticleShape::Mesh`,
// KUKI_PARTICLE_SHAPE_MESH, are not written here: the C++ decides both and compiles this with them.
// See `SHADER_DEFINITIONS`.
// Forty-four bytes. A particle's size and colour are not here because they are functions of how far
// through its life it is, and the two floats that say so already are: the vertex shader works both
// out from `age / lifetime`. Storing them would hold values every particle of an emitter derives the
// same way.
//
// The tint is the one thing a particle carries that its emitter cannot say for it: what the light
// made of the surface it was born on, which is different for every point of a mesh and has nowhere
// else to live once the particle has left. One everywhere for a particle that was born on nothing.
//
// A slot is dead when its age has reached its lifetime, which a lifetime of nought makes true of a
// slot that has never held anything. There is no flag, and no list of which slots are free. An age
// below nought is a particle being held: born, drawn, and not yet let go. See `u_hold`.
struct Particle {
  float3 position;
  float age;
  float3 velocity;
  float lifetime;
  float3 tint;
};
// One point on a mesh's surface, in the mesh's own space. Mirrors DXParticleSample in
// dx_renderer.cpp, which scatters as many of these as the pool has slots.
struct SurfaceSample {
  float3 position;
  float3 normal;
};
// Mirrors DXParticleUpdateConstants in dx_pipeline.hpp. Thirteen float4 rows, with every float3 at
// the start of one so that HLSL inserts no padding the C++ side does not also have.
cbuffer UpdateConstants : register(b0) {
  float3 u_emitterPosition;
  float u_deltaTime;
  // Where the emitter was a frame ago. Spawns are spread along the line between the two, so that an
  // emitter moving quickly leaves a trail rather than a row of clumps one frame apart.
  float3 u_previousPosition;
  float u_radius;
  // The direction particles are thrown in, unit length, and the cosine of the cone's half-angle.
  float3 u_axis;
  float u_spreadCosine;
  float3 u_gravity;
  float u_drag;
  float u_lifetime;
  float u_lifetimeVariance;
  float u_speed;
  float u_speedVariance;
  uint u_capacity;
  // The slots to refill this frame: `u_spawnCount` of them starting at `u_spawnStart` and wrapping.
  uint u_spawnStart;
  uint u_spawnCount;
  uint u_seed;
  // Set for a pool's first frame, whose buffer holds whatever the allocation left in it.
  uint u_reset;
  uint u_shape;
  // The longest a particle waits where it was born, and the fraction of that it may fall short by.
  float u_hold;
  float u_holdVariance;
  // The emitter's world matrix, as its first three rows. Read only by a particle born on a mesh,
  // which is placed by all of it; one born in the sphere needs the position and the axis above and
  // nothing else. Three rows because the fourth of an affine matrix says nothing.
  float4 u_world[3];
  // Where the sun is, what arrives from it, and what arrives from everywhere else. What a particle
  // born on a mesh is tinted by, once, as it is born.
  float3 u_sunDirection;
  float u_updatePadding0;
  float3 u_sunRadiance;
  float u_updatePadding1;
  float3 u_ambient;
  float u_updatePadding2;
};
// Mirrors DXParticleDrawConstants in dx_pipeline.hpp. Eight rows, on the same terms.
//
// At `b1` rather than sharing `b0` with the block above. No entry point reads both, so the two could
// overlap, but two blocks at one register in one file is a thing that works until somebody adds a
// stage that wants a value from each.
cbuffer DrawConstants : register(b1) {
  float4x4 u_viewProjection;
  float3 u_cameraRight;
  float u_startSize;
  float3 u_cameraUp;
  float u_endSize;
  float3 u_startColor;
  float u_intensity;
  float3 u_endColor;
  // One for particles that are added to the picture and nought for solid ones. What it switches is
  // the fade: an additive particle dims to nothing as it ends, and a solid one that dimmed would
  // only turn black, so it is left its colour and goes by shrinking.
  float u_fade;
};
RWStructuredBuffer<Particle> g_particles : register(u0);
StructuredBuffer<Particle> g_drawn : register(t0);
// One sample a slot, so a slot is always born at the same point of the mesh. At `t1` to stay clear
// of the buffer above, which belongs to a different stage and a different root signature.
StructuredBuffer<SurfaceSample> g_samples : register(t1);
// PCG, one round. Enough to decorrelate neighbouring indices, which is all that is asked of it: the
// values choose where a spark starts and which way it goes, and nothing downstream integrates them.
uint Hash(uint value) {
  uint state = value * 747796405u + 2891336453u;
  uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
  return (word >> 22u) ^ word;
}
float Random(inout uint state) {
  state = Hash(state);
  return float(state >> 8u) * (1.0 / 16777216.0);
}
// A direction inside the cone around an axis, uniform over the cap of the sphere the cone cuts out.
//
// Uniform in the cosine rather than in the angle, which is what makes it uniform over the cap: equal
// steps of angle bunch the directions up around the axis, and a spray thrown that way has a bright
// core that is an artefact of the sampling rather than of anything about the emitter.
float3 ConeDirection(float3 axis, inout uint state) {
  float cosTheta = lerp(1.0, u_spreadCosine, Random(state));
  float sinTheta = sqrt(saturate(1.0 - cosTheta * cosTheta));
  float phi = TWO_PI * Random(state);
  // Any vector not parallel to the axis will do to build a frame from, and the choice is invisible:
  // it only decides where `phi` starts counting, and `phi` is uniform all the way round.
  float3 helper = abs(axis.y) < 0.99 ? float3(0.0, 1.0, 0.0) : float3(1.0, 0.0, 0.0);
  float3 tangent = normalize(cross(helper, axis));
  float3 bitangent = cross(axis, tangent);
  return tangent * (sinTheta * cos(phi)) + bitangent * (sinTheta * sin(phi)) + axis * cosTheta;
}
// A point inside the unit sphere, uniform by volume. The cube root is what makes it so: without it
// the points are uniform in radius and crowd the centre.
float3 SpherePoint(inout uint state) {
  float z = Random(state) * 2.0 - 1.0;
  float ring = sqrt(saturate(1.0 - z * z));
  float phi = TWO_PI * Random(state);
  return float3(ring * cos(phi), z, ring * sin(phi)) * pow(Random(state), 1.0 / 3.0);
}
Particle Spawn(uint index) {
  uint state = Hash(index ^ u_seed);
  Particle particle;
  // How far through the frame this one was born. Every particle spawned in a frame is written by
  // the same dispatch, but they were not all emitted at the same instant: a rate of six hundred a
  // second over a sixtieth of a second is ten particles a few milliseconds apart. Giving each its
  // own moment -- a place along the emitter's path, and the part of the frame it has already been
  // flying for -- is what turns ten stacked on one point into a continuous stream.
  float moment = Random(state);
  float flown = (1.0 - moment) * u_deltaTime;
  float3 origin;
  float3 axis = u_axis;
  particle.tint = float3(1.0, 1.0, 1.0);
  if (u_shape == KUKI_PARTICLE_SHAPE_MESH) {
    // The slot's own point, every time. Which point that is was settled when the samples were
    // scattered, so a burst that fills the pool covers the mesh exactly as evenly as they do.
    SurfaceSample surface = g_samples[index];
    float4 local = float4(surface.position, 1.0);
    origin = float3(dot(u_world[0], local), dot(u_world[1], local), dot(u_world[2], local));
    // By the matrix itself rather than by its inverse transpose. The two agree for any scale that
    // is the same along every axis, and the error under one that is not is a tilt in a direction
    // that is about to have a random cone thrown around it.
    float3 normal = float3(dot(u_world[0].xyz, surface.normal), dot(u_world[1].xyz, surface.normal), dot(u_world[2].xyz, surface.normal));
    axis = dot(normal, normal) > 1.0e-12 ? normalize(normal) : u_axis;
    particle.tint = u_ambient + u_sunRadiance * saturate(dot(axis, u_sunDirection));
  } else
    origin = lerp(u_previousPosition, u_emitterPosition, moment) + SpherePoint(state) * u_radius;
  particle.velocity = ConeDirection(axis, state) * (u_speed * (1.0 - u_speedVariance * Random(state)));
  // Below nought for as long as it is to be held, and counted up through nought like any other age.
  // Only the part of the frame left over after the hold is flown, which with any hold at all is none
  // of it.
  particle.age = flown - u_hold * (1.0 - u_holdVariance * Random(state));
  particle.position = origin + particle.velocity * max(particle.age, 0.0);
  // Never nought, which is the value that means a slot is empty.
  particle.lifetime = max(u_lifetime * (1.0 - u_lifetimeVariance * Random(state)), 1.0e-3);
  return particle;
}
[numthreads(KUKI_PARTICLE_GROUP_SIZE, 1, 1)]
void CSSimulate(uint3 thread : SV_DispatchThreadID) {
  uint index = thread.x;
  // The last group runs past the end of a pool whose size is not a multiple of the group's.
  if (index >= u_capacity)
    return;
  Particle particle = g_particles[index];
  if (u_reset != 0u) {
    particle.position = float3(0.0, 0.0, 0.0);
    particle.velocity = float3(0.0, 0.0, 0.0);
    particle.age = 0.0;
    particle.lifetime = 0.0;
    particle.tint = float3(0.0, 0.0, 0.0);
  }
  // The pool is a ring and this frame's spawns are the next `u_spawnCount` slots round it. Asking
  // how far past the start a slot is, modulo the pool, answers whether it is one of them without a
  // branch for the range that wraps -- and without any thread having to claim a slot from a counter
  // the others are also reaching for, which is the whole reason for spawning this way.
  uint offset = (index + u_capacity - u_spawnStart) % u_capacity;
  if (offset < u_spawnCount)
    particle = Spawn(index);
  else if (particle.age < 0.0)
    // Held. Time passes and nothing else does: no gravity gathers on a particle that has not been
    // let go, so it leaves at the speed it was born with whenever that turns out to be.
    particle.age += u_deltaTime;
  else if (particle.age < particle.lifetime) {
    // Semi-implicit Euler: the velocity is stepped first and the position moved by the result. It
    // costs nothing over the explicit order and does not gain energy, which over a few seconds of
    // flight is the difference between an arc and an arc that lands somewhere it should not.
    particle.velocity += u_gravity * u_deltaTime;
    // The exact decay for a force proportional to speed, rather than a multiply by one minus the
    // product. That one goes negative when a long frame meets a high drag and throws the particle
    // backwards; this one only ever approaches rest.
    particle.velocity *= exp(-u_drag * u_deltaTime);
    particle.position += particle.velocity * u_deltaTime;
    particle.age += u_deltaTime;
  }
  g_particles[index] = particle;
}
struct PSInput {
  float4 position : SV_POSITION;
  float2 corner : TEXCOORD0;
  float3 color : COLOR0;
};
// Four vertices to a particle and no vertex buffer: the quad's corner comes out of the vertex index
// and the particle out of the instance index. Drawn as a strip, so the two bits of the index are the
// two axes of the corner.
PSInput VSMain(uint vertex : SV_VertexID, uint instance : SV_InstanceID) {
  PSInput output;
  Particle particle = g_drawn[instance];
  output.corner = float2(float(vertex & 1u), float(vertex >> 1u)) * 2.0 - 1.0;
  output.color = float3(0.0, 0.0, 0.0);
  // An empty slot, drawn as every vertex at one point so it covers no pixel at all. The instance
  // count is the whole pool because which slots hold a particle is decided on the GPU, and the
  // alternative to collapsing the dead ones here is telling the processor how many are alive.
  if (particle.age >= particle.lifetime) {
    output.position = float4(0.0, 0.0, 0.0, 1.0);
    return output;
  }
  float life = saturate(particle.age / particle.lifetime);
  float size = lerp(u_startSize, u_endSize, life);
  // Spanned by the camera's own axes, which is what keeps the quad facing it. Every quad in the
  // frame is parallel to the image plane rather than turned towards the eye, and at the size of a
  // spark the two cannot be told apart.
  float3 world = particle.position + (u_cameraRight * output.corner.x + u_cameraUp * output.corner.y) * (size * 0.5);
  output.position = mul(u_viewProjection, float4(world, 1.0));
  // Fades out as well as changing colour, and faster towards the end. Without it a particle whose
  // end size is not nought is at full brightness in one frame and gone in the next, and the eye
  // catches a hundred of those a second as flicker.
  output.color = lerp(u_startColor, u_endColor, life) * particle.tint * (u_intensity * (1.0 - u_fade * life * life));
  return output;
}
// A soft disc with no texture behind it: bright at the centre, nothing at the quad's inscribed
// circle, and squared in between so the edge arrives gently. The corners of the quad are outside the
// circle and add nought, which under an additive blend is the same as not having been drawn.
float4 PSMain(PSInput input) : SV_TARGET {
  float falloff = saturate(1.0 - dot(input.corner, input.corner));
  return float4(input.color * (falloff * falloff), 0.0);
}
// The solid kind: the same disc with a hard edge, in one flat colour. The corners are thrown away
// rather than blended to nothing, because this one writes depth and a corner that wrote depth would
// hide whatever was behind a part of the quad nobody can see.
float4 PSOpaque(PSInput input) : SV_TARGET {
  clip(1.0 - dot(input.corner, input.corner));
  return float4(input.color, 0.0);
}
