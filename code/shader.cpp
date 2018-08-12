
#define HLSL(src) "" #src

struct PrimitiveShaderVars {
	Mat4 view;
	Mat4 proj;
	Vec4 color;
	int gammaGradient;

	Vec3 padding;
};

char* d3dPrimitiveShader = HLSL (

	struct VSInput {
		float3 pos : POSITION;
		float4 color : COLOR;
		float2 uv : TEXCOORD;
	};

	struct PSInput {
		float4 pos : SV_POSITION;
		float4 color : COLOR;
		float2 uv : TEXCOORD;
	};

	struct ShaderVars {
		float4x4 view;
		float4x4 proj;
		float4 color;
		int gammaGradient;

		float3 padding;
	};

	Texture2D    tex  : register(t0);
	SamplerState samp : register(s0);
	ShaderVars   vars : register(b0);

	PSInput vertexShader(VSInput input) {
		PSInput output;

		output.pos = mul(mul(float4(input.pos, 1), vars.view), vars.proj);

		if(!vars.gammaGradient) {
			output.color = float4(pow(input.color.rgb, 2.2f), input.color.a) * 
			               float4(pow(vars.color.rgb, 2.2f), vars.color.a);
		} else {
			output.color = input.color * vars.color;
		}

		output.uv = input.uv;

		return output;
	}

	float4 pixelShader(PSInput input) : SV_Target {
		float4 color = tex.Sample(samp, input.uv) * input.color;
		if(vars.gammaGradient) {
			color.rgb = pow(color.rgb, 2.2f);
		}
		return color;
	}
);

//

// __declspec(align(16)) 
// #pragma pack(push,16)

struct Light {
	int type;
	Vec3 dir;
	Vec3 color;
	float _pad0;
};

struct ShaderMaterial {
	int smoothing;
	Vec3 Ka; // ambient
	Vec3 Kd; // diffuse
	float _pad1;
	Vec3 Ks; // specular
	float Ns; // shininess / specularExponent

	Vec3 Ke;
	float Ni;
	float d;
	int illum;
	int hasBumpMap;
	float _pad2;

	// Vec3 ambient;           // Ka 1.0 1.0 1.0
	// Vec3 diffuse;           // Kd 0.0 0.0 0.0
	// Vec3 specular;          // Ks 0.9 0.9 0.9
	// float specularExponent; // Ns 96.0
	// Vec3 emission;          // Ke 0.0 0.0 0.0
	// float refractionIndex;  // Ni 1.0
	// float alpha;            // d 1.0
	// int illuminationMode;   // illum 2
};

struct MVPMatrix {
	Mat4 model;
	Mat4 view;
	Mat4 proj;
};

struct ShaderSamples {
	Vec4 data[16];
	int count;

	Vec3 padding;
};

struct MainShaderVars {
	ShaderSamples samples;
	MVPMatrix mvp;
	Vec4 color;
	Vec3 camPos;
	float _pad0;

	Vec3 ambient;
	float _pad1;
	Light light;
	float value;
	int lightCount;
	Vec2 shadowMapSize;

	ShaderMaterial material;

	MVPMatrix mvpTexProj;
	Vec4 asdf;
};

char* d3dMainShader = HLSL (

	struct VSInput {
		float3 pos       : POSITION;
		float2 uv        : TEXCOORD;
		float3 normal    : NORMAL0;
		float3 tangent   : NORMAL1;
		float3 bitangent : NORMAL2;
	};

	struct PSInput {
		float4 pos : SV_POSITION;
		float4 color : COLOR;
		float2 uv : TEXCOORD0;

		float3 normal : NORMAL0;
		float3 tangent : NORMAL1;
		float3 bitangent : NORMAL2;

		float3 look : NORMAL3;
		float3 half : NORMAL4;

		float4 posTexProj : POSITION0;
		float4 position : POSITION1;
	};

	struct Light {
		int type;
		float3 dir;
		float3 color;
		float pad0;
	};

	struct Material {
		int smoothing;
		float3 Ka;
		float3 Kd;
		float3 Ks;
		float Ns;

		float3 Ke;
		float Ni;
		float d;
		int illum;
		int hasBumpMap;

		float _pad1;
	};

	struct MVPMatrix {
		float4x4 model;
		float4x4 view;
		float4x4 proj;
	};

	struct ShaderSamples {
		float4 data[16];
		int count;
	};

	struct ShaderVars {
		ShaderSamples samples;
		MVPMatrix mvp;
		float4 color;
		float3 camPos;

		float3 ambient;
		float _pad0;
		Light light;
		float value;
		int lightCount;
		float2 shadowMapSize;

		Material material;

		MVPMatrix mvpTexProj;
		float4 asdf;
	};

	Texture2D     tex   : register(t0);
	Texture2D     bump  : register(t1);
	Texture2D     rough : register(t2);
	Texture2D     pTex  : register(t5);
	ShaderVars    vars  : register(b0);
	SamplerState  samp  : register(s0);
	SamplerComparisonState sampP : register(s1);

	PSInput vertexShader(VSInput input) {
		PSInput output;

		float4 posWorldSpace = mul(float4(input.pos, 1), vars.mvp.model);
		output.pos = mul(mul(posWorldSpace, vars.mvp.view), vars.mvp.proj);

		float4 color = float4(pow(vars.color.rgb, 2.2f), vars.color.a);
		output.color = color;
		output.color.a *= vars.material.d;
		output.uv = input.uv;

		output.normal = mul(input.normal, vars.mvp.model);
		output.look = vars.camPos - posWorldSpace.xyz;

		if(vars.material.hasBumpMap) {
			output.tangent   = mul(input.tangent, vars.mvp.model);
			output.bitangent = mul(input.bitangent, vars.mvp.model);
		}

		output.posTexProj = mul(mul(mul(float4(input.pos, 1), vars.mvp.model), vars.mvpTexProj.view), vars.mvpTexProj.proj);

		output.position = output.pos;

		return output;
	}

	float SampleShadowMap(in float2 base_uv, in float u, in float v, in float2 shadowMapSizeInv,
	                      in uint cascadeIdx,  in float depth, in float2 receiverPlaneDepthBias) {

	    float2 uv = base_uv + float2(u, v) * shadowMapSizeInv;

        // float z = depth + dot(float2(u, v) * shadowMapSizeInv, receiverPlaneDepthBias);
        float z = depth;

	    return pTex.SampleCmpLevelZero(sampP, uv, z);
	}

	float SampleShadowMapOptimizedPCF(float3 shadowPos, float3 shadowPosDX, float3 shadowPosDY, uint cascadeIdx, float2 receiverPlaneDepthBias) {
		float2 shadowMapSize = vars.shadowMapSize;

		float lightDepth = shadowPos.z;

		float2 uv = shadowPos.xy * shadowMapSize; // 1 unit - 1 texel

		float2 shadowMapSizeInv = 1.0 / shadowMapSize;

		float2 base_uv;
		base_uv.x = floor(uv.x + 0.5);
		base_uv.y = floor(uv.y + 0.5);

		float s = (uv.x + 0.5 - base_uv.x);
		float t = (uv.y + 0.5 - base_uv.y);

		base_uv -= float2(0.5, 0.5);
		base_uv *= shadowMapSizeInv;

		
		float uw0 = (4 - 3 * s);
		float uw1 = 7;
		float uw2 = (1 + 3 * s);

		float u0 = (3 - 2 * s) / uw0 - 2;
		float u1 = (3 + s) / uw1;
		float u2 = s / uw2 + 2;

		float vw0 = (4 - 3 * t);
		float vw1 = 7;
		float vw2 = (1 + 3 * t);

		float v0 = (3 - 2 * t) / vw0 - 2;
		float v1 = (3 + t) / vw1;
		float v2 = t / vw2 + 2;


		float sum = 0;
		sum += uw0 * vw0 * SampleShadowMap(base_uv, u0, v0, shadowMapSizeInv, cascadeIdx, lightDepth, receiverPlaneDepthBias);
		sum += uw1 * vw0 * SampleShadowMap(base_uv, u1, v0, shadowMapSizeInv, cascadeIdx, lightDepth, receiverPlaneDepthBias);
		sum += uw2 * vw0 * SampleShadowMap(base_uv, u2, v0, shadowMapSizeInv, cascadeIdx, lightDepth, receiverPlaneDepthBias);

		sum += uw0 * vw1 * SampleShadowMap(base_uv, u0, v1, shadowMapSizeInv, cascadeIdx, lightDepth, receiverPlaneDepthBias);
		sum += uw1 * vw1 * SampleShadowMap(base_uv, u1, v1, shadowMapSizeInv, cascadeIdx, lightDepth, receiverPlaneDepthBias);
		sum += uw2 * vw1 * SampleShadowMap(base_uv, u2, v1, shadowMapSizeInv, cascadeIdx, lightDepth, receiverPlaneDepthBias);

		sum += uw0 * vw2 * SampleShadowMap(base_uv, u0, v2, shadowMapSizeInv, cascadeIdx, lightDepth, receiverPlaneDepthBias);
		sum += uw1 * vw2 * SampleShadowMap(base_uv, u1, v2, shadowMapSizeInv, cascadeIdx, lightDepth, receiverPlaneDepthBias);
		sum += uw2 * vw2 * SampleShadowMap(base_uv, u2, v2, shadowMapSizeInv, cascadeIdx, lightDepth, receiverPlaneDepthBias);

		return sum * 1.0f / 144;
	}

	float3 calculateLight(PSInput input) {

		// Sample shadow.
		float shadowFactor;
		{
			float3 projCoord = input.posTexProj.xyz / input.posTexProj.w;

			if(projCoord.x < -1.0f || projCoord.x > 1.0f || 
			   projCoord.y < -1.0f || projCoord.y > 1.0f || 
			   projCoord.z <  0.0f || projCoord.z > 1.0f) {
				// return vars.material.Ka * vars.ambient;
				return float3(0,1,0);
			}

			projCoord.y *= -1;
			projCoord.xy = (projCoord.xy + 1) / 2.0f;

			// Calculate bias.

			float bias = 0;
			float receiverPlaneDepthBias;
			if(false)
			{
				// Constant, slope-scale, normal and receiver plane depth biases.

				float normalBiasScale;
				float slopeBiasScale;
				{
					{
						float3 N = normalize(input.normal);
						float3 L = -vars.light.dir;

						float cos_alpha = saturate(dot(N, L));
						normalBiasScale = sqrt(1 - cos_alpha*cos_alpha); // sin(acos(L·N))
						slopeBiasScale = normalBiasScale / cos_alpha;    // tan(acos(L·N))
						slopeBiasScale = min(2, slopeBiasScale);
					}

					{
						float3 texCoordDX = ddx(projCoord);
						float3 texCoordDY = ddy(projCoord);

						float2 biasUV;
						biasUV.x = texCoordDY.y * texCoordDX.z - texCoordDX.y * texCoordDY.z;
						biasUV.y = texCoordDX.x * texCoordDY.z - texCoordDY.x * texCoordDX.z;
						biasUV *= 1.0f / ((texCoordDX.x * texCoordDY.y) - (texCoordDX.y * texCoordDY.x));

						receiverPlaneDepthBias = biasUV;
					}
				}

				bias += 0.002f * normalBiasScale;
				bias += 0.002f * slopeBiasScale;

				// // Static depth biasing to make up for incorrect fractional sampling on the shadow map grid
				// float2 texelSize = 1.0f / vars.shadowMapSize;
				// float fractionalSamplingError = 2 * dot(float2(1.0f, 1.0f) * texelSize, abs(receiverPlaneDepthBias));

				// bias += min(fractionalSamplingError, 0.01f);
			}

			// projCoord.z -= bias;

			shadowFactor = 0.0f;
			{
				// Single PCF.
				{
					// shadowFactor = pTex.SampleCmpLevelZero(sampP, projCoord.xy, projCoord.z);
				}

				// 4x4 PCF grid.
				{
					// for(float y = -1.5f; y <= 1.5f; y += 1.0f) {
					// 	for(float x = -1.5f; x <= 1.5f; x += 1.0f) {
					// 		float2 uvOffset = float2(x * 1/vars.shadowMapSize.x, y * 1/vars.shadowMapSize.y);
					// 		shadowFactor += pTex.SampleCmpLevelZero(sampP, projCoord.xy + uvOffset, projCoord.z);
					// 	}
					// }
					// shadowFactor /= 16.0f;
				}

				// Poissong disk 16 samples.
				{
					for(int i = 0; i < vars.samples.count; i++) {
						// float2 sample = vars.samples.data[i] * 1.5f;
						float2 sample = vars.samples.data[i] * 1.5f;
						float2 uvOffset = sample * (1 / vars.shadowMapSize);

						shadowFactor += pTex.SampleCmpLevelZero(sampP, projCoord.xy + uvOffset, projCoord.z);

					}
					shadowFactor /= 16.0f;
				}

				// The Witness sampling.
				{
					// shadowFactor = SampleShadowMapOptimizedPCF(projCoord, 0, 0, 0, receiverPlaneDepthBias);
				}
			}

			if(shadowFactor == 0.0f) return vars.material.Ka * vars.ambient;
		}

		float3 lightDir = -vars.light.dir;
		float3 normal;

		if(vars.material.hasBumpMap) {
			float3 bumpColor = bump.Sample(samp, input.uv).rgb;
			float3 bumpNormal = normalize((bumpColor*2)-1);

			float3x3 tbn = float3x3(normalize(input.bitangent), 
			                        normalize(input.tangent), 
			                        normalize(input.normal));

			normal = mul(bumpNormal, tbn);

		} else {
			normal = normalize(input.normal);
		}

		float3 roughness = rough.Sample(samp, input.uv).rgb;
		float3 specularColor = vars.material.Ks * roughness.rgb;

		float3 half = normalize(lightDir + normalize(input.look));

		// Blinn-Phong.
		float3 lightIntensity;
		{
			Material m = vars.material;

			float3 Ia = m.Ka * vars.ambient;
			float3 Id = m.Kd * saturate(dot(normal, lightDir));
			float3 Is = specularColor * pow(saturate(dot(normal, half)), m.Ns);

			lightIntensity = Ia + shadowFactor * ((Id + Is) * vars.light.color);
		}

		return lightIntensity;
	}

	float4 pixelShader(PSInput input) : SV_Target {
		float4 output;

		// // output.a = round(output.a);
		// // if(output.a != 0) output.a = 1;
		// clip(output.a <= 0.5f ? -1:1);

      // float4 texColor = tex.Sample(samp, input.uv);
		// clip(texColor.a == 0.0f ? -1:1);

		float3 lightIntensity = calculateLight(input);

		output = input.color * 
		         tex.Sample(samp, input.uv) * 
		         float4(lightIntensity, 1);

		return output;
	}
);

//

// http://codeflow.org/entries/2011/apr/13/advanced-webgl-part-2-sky-rendering/

struct SkyShaderVars {
	Mat4 viewInv;
	Mat4 projInv;

	Vec3 sunDir;

	float spotBrightness;
	float mieBrightness;
	float mieDistribution;
	float mieStrength;
	float mieCollectionPower;
	float rayleighBrightness;
	float rayleighStrength;
	float rayleighCollectionPower;
	float scatterStrength;
	float surfaceHeight;
	float intensity;
	int   stepCount;
	float horizonOffset;
	float sunOffset;

	Vec3 padding;
};

char* d3dSkyShader = HLSL (

	struct VSInput {
		uint id : SV_VertexID;
	};

	struct PSInput {
		float4 pos : SV_POSITION;
		float4 p : POSITION;
	};

	struct ShaderVars {
		float4x4 viewInv;
		float4x4 projInv;

		float3 sunDir;

		float spotBrightness;
		float mieBrightness;
		float mieDistribution;
		float mieStrength;
		float mieCollectionPower;
		float rayleighBrightness;
		float rayleighStrength;
		float rayleighCollectionPower;
		float scatterStrength;
		float surfaceHeight;
		float intensity;
		int   stepCount;
		float horizonOffset;
		float sunOffset;

		float3 padding;
	};

	ShaderVars vars : register(b0);

	PSInput vertexShader(VSInput input) {
		PSInput output;

		float2 verts[4];
		verts[0] = float2( -1, -1 );
		verts[1] = float2( -1,  1 );
		verts[2] = float2(  1, -1 );
		verts[3] = float2(  1,  1 );

		output.pos = float4(verts[input.id], 0, 1);
		output.p = output.pos;

		return output;
	}

	float phase(float alpha, float g){
	    float a = 3.0*(1.0-g*g);
	    float b = 2.0*(2.0+g*g);
	    float c = 1.0+alpha*alpha;
	    float d = pow(1.0+g*g-2.0*g*alpha, 1.5);
	    return (a/b)*(c/d);
	}

	float atmospheric_depth(float3 position, float3 dir){
	    float a = dot(dir, dir);
	    float b = 2.0*dot(dir, position);
	    float c = dot(position, position)-1.0;
	    float det = b*b-4.0*a*c;
	    float detSqrt = sqrt(det);
	    float q = (-b - detSqrt)/2.0;
	    float t1 = c/q;
	    return t1;
	}

	float horizon_extinction(float3 position, float3 dir, float radius){
	    float u = dot(dir, -position);
	    if(u<0.0){
	        return 1.0;
	    }
	    float3 near = position + u*dir;
	    if(length(near) < radius){
	        return 0.0;
	    }
	    else{
	        float3 v2 = normalize(near)*radius - position;
	        float diff = acos(dot(normalize(v2), dir));

	        // @TODO: Some wrong pixels when using smoothstep with 0.0f as min.
	        return smoothstep(0.00001f, 1.0, pow(diff*2.0, 3.0));
	        // return smoothstep(0.0, 1.0, pow(diff*1.0, 3.0));
	    }
	}

	static float3 Kr = float3(
	    0.18867780436772762, 0.4978442963618773, 0.6616065586417131
	);

	float3 absorb(float dist, float3 color, float factor){
		float3 test = pow(Kr, float3(factor/dist, factor/dist, factor/dist));

		return color-color*test;
	}

	float4 pixelShader(PSInput input) : SV_Target {
		float2 p = input.p.xy;
		// float4 p = SV_Position;

		float4 deviceNormal = float4(p, 0.0, 1.0);
		float3 normal = normalize(mul(deviceNormal, vars.projInv)).xyz; 
		normal = normalize(mul(float4(normal,0), vars.viewInv));

		float3 n = normal;

		n = (n + 1) / 2;

		// float3 lightdir = normalize(float3(0.25f,0,1));
		float3 lightdir = vars.sunDir;

		float spot_brightness           = vars.spotBrightness;

		float mie_brightness            = vars.mieBrightness;
		float mie_distribution          = vars.mieDistribution;
		float mie_strength              = vars.mieStrength;
		float mie_collection_power      = vars.mieCollectionPower;

		float rayleigh_brightness       = vars.rayleighBrightness;
		float rayleigh_strength         = vars.rayleighStrength;
		float rayleigh_collection_power = vars.rayleighCollectionPower;

		float scatter_strength          = vars.scatterStrength;

		//

		float surface_height = vars.surfaceHeight;
		float intensity = vars.intensity;
		int step_count = vars.stepCount;

		float horizonOffset = vars.horizonOffset;
		float sunOffset = vars.sunOffset;

		//

		float3 eyedir = normal;
		float alpha = dot(eyedir, lightdir);

		float rayleigh_factor = phase(alpha, -0.01)*rayleigh_brightness;
		float mie_factor = phase(alpha, mie_distribution)*mie_brightness;
		float spot = smoothstep(0.0, 15.0, phase(alpha, 0.9995))*spot_brightness;

		float3 eye_position = float3(0.0, 0, surface_height);
		float eye_depth = atmospheric_depth(eye_position, eyedir);
		float step_length = eye_depth / float(step_count);

		float eye_extinction = horizon_extinction(eye_position, eyedir, surface_height-horizonOffset);

		float3 rayleigh_collected = float3(0.0, 0.0, 0.0);
		float3 mie_collected = float3(0.0, 0.0, 0.0);

		for(int i=0; i<step_count; i++){
		    float sample_distance = step_length*float(i);
		    float3 position = eye_position + eyedir*sample_distance;
		    float extinction = horizon_extinction(position, lightdir, surface_height-sunOffset);
		    float sample_depth = atmospheric_depth(position, lightdir);

		    //

			float3 influx = absorb(sample_depth, float3(intensity, intensity, intensity), scatter_strength)*extinction;

			rayleigh_collected += absorb(sample_distance, Kr*influx, rayleigh_strength);
			mie_collected += absorb(sample_distance, influx, mie_strength);
		}

		rayleigh_collected = (rayleigh_collected * eye_extinction * pow(eye_depth, rayleigh_collection_power)) / float(step_count);

		mie_collected = (mie_collected * eye_extinction * pow(eye_depth, mie_collection_power)) / float(step_count);

		float3 color = float3(spot*mie_collected + mie_factor*mie_collected + rayleigh_factor*rayleigh_collected);

		return float4(pow(color,2.2f), 1);
	}
);

//

struct GradientShaderVars {
	Mat4 view;
	Mat4 proj;

	Vec4 color0;
	Vec4 color1;

	float value;

	Vec3 padding;
};

char* d3dGradientShader = HLSL (

	struct VSInput {
		float3 pos   : POSITION;
		float4 color : COLOR;
		float2 uv    : TEXCOORD;
	};

	struct PSInput {
		float4 pos : SV_POSITION;
		float2 uv : TEXCOORD;
	};

	struct ShaderVars {
		float4x4 view;
		float4x4 proj;

		float4 color0;
		float4 color1;

		float value;

		float3 padding;
	};

	ShaderVars vars : register(b0);

	PSInput vertexShader(VSInput input) {
		PSInput output;

		output.pos = mul(mul(float4(input.pos, 1), vars.view), vars.proj);
		output.uv = input.uv;

		return output;
	}

	float4 pixelShader(PSInput input) : SV_Target {
		float4 color;

		color = vars.color0 *    input.uv.y + 
		        vars.color1 * (1-input.uv.y);
		color.rgb = pow(color.rgb, vars.value);

		return color;
		// return float4(1,1,1,1);
	}
);

//

struct CubeShaderVars {
	Mat4 view;
	Mat4 proj;
};

char* d3dCubeShader = HLSL (

	struct VSInput {
		float3 pos : POSITION;
	};

	struct PSInput {
		float4 pos      : SV_POSITION;
		float3 position : POSITION;
	};

	struct ShaderVars {
		float4x4 view;
		float4x4 proj;
	};

	TextureCube  tex  : register(t0);
	SamplerState samp : register(s0);
	ShaderVars   vars : register(b0);

	PSInput vertexShader(VSInput input) {
		PSInput output;

		output.pos = mul(mul(float4(input.pos, 1), vars.view), vars.proj);

		float3 p = input.pos;
		p.x *= -1;
		output.position = p;

		return output;
	}

	float4 pixelShader(PSInput input) : SV_Target {
		float4 output;

		float3 direction = normalize(input.position);
		output = tex.Sample(samp, direction);

		return output;
	}
);

//

struct ShadowShaderVars {
	Mat4 model;
	Mat4 view;
	Mat4 proj;
};

char* d3dShadowShader = HLSL (
	struct VSInput {
		float3 pos : POSITION;
		float2 uv  : TEXCOORD;
	};

	struct PSInput {
		float4 pos : SV_POSITION;
		float4 color : COLOR;
		float2 uv : TEXCOORD0;
	};

	struct ShaderVars {
		float4x4 model;
		float4x4 view;
		float4x4 proj;
	};

	ShaderVars vars   : register(b0);
	Texture2D  tex    : register(t0);
	SamplerState samp : register(s0);

	PSInput vertexShader(VSInput input) {
		PSInput output;

		output.pos = mul(mul(mul(float4(input.pos, 1), vars.model), vars.view), vars.proj);

		output.uv = input.uv;

		return output;
	}

	void pixelShader(PSInput input) {
      float4 texColor = tex.Sample(samp, input.uv);
		clip(texColor.a == 0.0f ? -1:1);
	}
);

//

#define ShaderList(func) \
	func(Primitive) \
	func(Main) \
	func(Sky) \
	func(Gradient) \
	func(Cube) \
	func(Shadow) \

#define funcc(name) Shader_##name,
enum {
	Shader_Pre = -1,
	ShaderList(funcc)
};
#undef funcc

struct ShaderInfo {
	char* code;
	int varsSize;
};

#define makeShaderInfo(name) { d3d##name##Shader, sizeof(name##ShaderVars) },

ShaderInfo shaderInfos[] = {
	ShaderList(makeShaderInfo)
};