#include "pch.h"
#include "../Defines.h"

#include "WaterMaterial.h"
#include "MaterialDefinition.h"
#include "MaterialFactory.h"

#ifndef ROAD_EDITOR
	#include "../../OgreGame.h"
#else
	#include "../../../editor/OgreApp.h"
#endif

#include <OgreMaterial.h>
#include <OgrePass.h>
#include <OgreTechnique.h>
#include <OgreHighLevelGpuProgramManager.h>
#include <OgreMaterialManager.h>
using namespace Ogre;

WaterMaterialGenerator::WaterMaterialGenerator()
{
	mName = "Water";
}

//----------------------------------------------------------------------------------------

void WaterMaterialGenerator::generate()
{
	mMaterial = prepareMaterial(mDef->getName());
	
	resetTexUnitCounter();
	
	// we need a lot of uniform constants, disabling shadows reduces them significantly so that it can still run on PS2
	const RenderSystemCapabilities *caps = Root::getSingleton().getRenderSystem()->getCapabilities();
	if(!caps->isShaderProfileSupported("ps_4_0") && !caps->isShaderProfileSupported("fp40"))
		mDef->mProps->receivesShadows = false;

	
	// -------------------------- Main technique ----------------------------- //
	Ogre::Technique* technique = mMaterial->createTechnique();

	//  Main pass -----------------------------------------------------------
	Ogre::Pass* pass = technique->createPass();
	
	pass->setCullingMode(CULL_NONE);
	pass->setDepthWriteEnabled(true);
	
#ifndef ROAD_EDITOR
	if (!mParent->getRefract() && !mParent->pApp->NeedMRTBuffer())
#else
	if (!mParent->getRefract())
#endif
		pass->setSceneBlending(SBT_TRANSPARENT_ALPHA);
	
	Ogre::TextureUnitState* tu;
	//  normal map
	mNormalMap = pickTexture(&mDef->mProps->normalMaps);
	tu = pass->createTextureUnitState( mNormalMap );
	tu->setName("normalMap");
	mNormalTexUnit = mTexUnit_i; mTexUnit_i++;

	// global terrain lightmap (static)
	if (needTerrainLightMap())
	{
		tu = pass->createTextureUnitState(""); // texture name set later (in changeShadows)
		tu->setName("terrainLightMap");
		mTerrainLightTexUnit = mTexUnit_i; mTexUnit_i++;
	}

#ifndef ROAD_EDITOR
	if (mParent->getRefract() || mParent->pApp->NeedMRTBuffer())
#else
	if (mParent->getRefract())
#endif
	{
		tu = pass->createTextureUnitState( "PlaneRefraction" );
		tu->setName("refractionMap");
		tu->setTextureAddressingMode(TextureUnitState::TAM_CLAMP);
		mScreenRefrUnit = mTexUnit_i++;
	}
	if (mParent->getReflect())
	{
		tu = pass->createTextureUnitState( "PlaneReflection" );
		tu->setName("reflectionMap");
		tu->setTextureAddressingMode(TextureUnitState::TAM_CLAMP);
		mScreenReflUnit = mTexUnit_i++;
	}
	//else
	{
		// retrieve sky texture name from scene
		std::string skyTexName;
		if (mParent->pApp->terrain)
		{
			MaterialPtr mtrSky = MaterialManager::getSingleton().getByName(mParent->pApp->sc.skyMtr);
			if(!mtrSky.isNull())
			{
				Pass* passSky = mtrSky->getTechnique(0)->getPass(0);
				TextureUnitState* tusSky = passSky->getTextureUnitState(0);

				skyTexName = tusSky->getTextureName();
			}
		}
		else skyTexName = String("white.png");
		
		tu = pass->createTextureUnitState( skyTexName );
		tu->setName("skyMap");
		tu->setTextureAddressingMode(TextureUnitState::TAM_MIRROR);
		mEnvTexUnit = mTexUnit_i++;
	}
	
	//  waterDepth
	tu = pass->createTextureUnitState( "" );
	tu->setName("depthMap");
#ifndef ROAD_EDITOR
	if (mParent->pApp->NeedMRTBuffer()) {
#else
	if (0) {
#endif
		tu->setTextureAddressingMode(TextureUnitState::TAM_CLAMP);
	}
	else
	{
		tu->setTextureAddressingMode(TextureUnitState::TAM_BORDER);
		tu->setTextureBorderColour(ColourValue::White);  // outside tex water always visible
	}
	mWaterDepthUnit = mTexUnit_i;  mTexUnit_i++;
	
	
	// realtime shadow maps
	if (needShadows())
	{
		mShadowTexUnit_start = mTexUnit_i;
		for (int i = 0; i < mParent->getNumShadowTex(); ++i)
		{
			tu = pass->createTextureUnitState();
			tu->setName("shadowMap" + toStr(i));
			tu->setContentType(TextureUnitState::CONTENT_SHADOW);
			tu->setTextureAddressingMode(TextureUnitState::TAM_BORDER);
			tu->setTextureBorderColour(ColourValue::White);
			mTexUnit_i++;
		}
	}
	
	// shader
	if (!mShaderCached)
	{
		mVertexProgram = createVertexProgram();
		mFragmentProgram = createFragmentProgram();
	}
	
	pass->setVertexProgram(mVertexProgram->getName());
	pass->setFragmentProgram(mFragmentProgram->getName());
	
	if (mShaderCached)
	{
		individualFragmentProgramParams(pass->getFragmentProgramParameters());
		individualVertexProgramParams(pass->getFragmentProgramParameters());
	}

	// ----------------------------------------------------------------------- //
	
	createSSAOTechnique();
	createOccluderTechnique();

	// indicate we need 'time' parameter set every frame
	mParent->timeMtrs.push_back(mDef->getName());
	mParent->softMtrs.push_back(mDef->getName());
	/*
	if (mDef->getName() == "Grease_jelly")
	{
		LogO("[MaterialFactory] Vertex program source: ");
		StringUtil::StrStreamType vSourceStr;
		generateVertexProgramSource(vSourceStr);
		LogO(vSourceStr.str());
		LogO("[MaterialFactory] Fragment program source: ");
		StringUtil::StrStreamType fSourceStr;
		generateFragmentProgramSource(fSourceStr);
		LogO(fSourceStr.str());
	}
	/**/
}

//----------------------------------------------------------------------------------------

void WaterMaterialGenerator::generateVertexProgramSource(Ogre::StringUtil::StrStreamType& outStream)
{
	mTexCoord_i = 5;
	outStream <<
	"struct VIn \n"
	"{ \n"
	"	float4 p : POSITION;	float3 n : NORMAL; \n"
	"	float3 t : TANGENT;		float3 uv: TEXCOORD0; \n"
	"}; \n"
	"struct VOut \n"
	"{ \n"
	"	float4 p : POSITION;	float3 uv : TEXCOORD0;	float4 wp : TEXCOORD1; \n"
	"	float4 n : TEXCOORD2;	float4 t  : TEXCOORD3;	float4 b  : TEXCOORD4; \n"
	"}; \n"
	"VOut main_vp(VIn IN, \n"
	"	uniform float4x4 wMat,  uniform float4x4 wvpMat, \n";
	
	vpShadowingParams(outStream);
	
	outStream <<
	"	uniform float4 fogParams) \n"
	"{ \n"
	"	VOut OUT; \n"
	"	OUT.wp = mul(wMat, IN.p); \n"
	"	OUT.p = mul(wvpMat, IN.p); \n";
	
	if (mParent->getReflect() || mParent->getRefract()) outStream <<
	"	float4x4 scalemat = float4x4(0.5,   0,   0, 0.5, "
	"                              	 0,		-0.5,   0, 0.5, "
	"							  	 0,  	0, 	 0.5, 0.5, "
	"							  	 0,  	0,   0,   1); \n"
	"	float4 texcoordProj = mul(scalemat, OUT.p); \n"
	"	OUT.n.w = texcoordProj.x; OUT.t.w = texcoordProj.y; OUT.b.w = texcoordProj.w; \n";
	
	outStream <<
	"	OUT.n.xyz = IN.n;  OUT.t.xyz = IN.t;  OUT.b.xyz = cross(IN.t, IN.n); \n"
	"	OUT.wp.w = saturate(fogParams.x * (OUT.p.z - fogParams.y) * fogParams.w); \n"
	"	OUT.uv.xyz = float3(IN.uv.x, IN.uv.y, OUT.p.z); \n";
	
	if (needShadows())
	{
		for (int i=0; i<mParent->getNumShadowTex(); ++i)
		{
			outStream << "oLightPosition"+toStr(i)+" = mul(texWorldViewProjMatrix"+toStr(i)+", IN.p); \n";
		}
	}
	
	outStream <<
	"	return OUT; \n"
	"} \n";
}

//----------------------------------------------------------------------------------------

void WaterMaterialGenerator::vertexProgramParams(Ogre::HighLevelGpuProgramPtr program)
{
	GpuProgramParametersSharedPtr params = program->getDefaultParameters();
	
	params->setIgnoreMissingParams(true);

	params->setNamedAutoConstant("wMat", GpuProgramParameters::ACT_WORLD_MATRIX);
	params->setNamedAutoConstant("wvpMat", GpuProgramParameters::ACT_WORLDVIEWPROJ_MATRIX);
	params->setNamedAutoConstant("fogParams", GpuProgramParameters::ACT_FOG_PARAMS);
	
	individualVertexProgramParams(params);
}

//----------------------------------------------------------------------------------------

void WaterMaterialGenerator::individualVertexProgramParams(Ogre::GpuProgramParametersSharedPtr params)
{
	params->setIgnoreMissingParams(true); //! workaround 'Parameter texWorldViewProjMatrix0 does not exist' - no idea why its happening
	if (needShadows())
	for (int i=0; i<mParent->getNumShadowTex(); ++i)
	{
		params->setNamedAutoConstant("texWorldViewProjMatrix"+toStr(i), GpuProgramParameters::ACT_TEXTURE_WORLDVIEWPROJ_MATRIX, i);
	}
}

//----------------------------------------------------------------------------------------

HighLevelGpuProgramPtr WaterMaterialGenerator::createVertexProgram()
{
	HighLevelGpuProgramManager& mgr = HighLevelGpuProgramManager::getSingleton();
	std::string progName = mDef->getName() + "_VP";

	HighLevelGpuProgramPtr ret = mgr.getByName(progName);
	if (!ret.isNull())
		mgr.remove(progName);

	ret = mgr.createProgram(progName, ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME, 
		"cg", GPT_VERTEX_PROGRAM);

	if(MRTSupported())
	{
		ret->setParameter("profiles", "vs_4_0 vs_3_0 vp40");
	}
	else
	{
		ret->setParameter("profiles", "vs_4_0 vs_2_x vp40 arbvp1");
	}
	ret->setParameter("entry_point", "main_vp");

	StringUtil::StrStreamType outStream;
	generateVertexProgramSource(outStream);
	ret->setSource(outStream.str());
	ret->load();

	vertexProgramParams(ret);
	
	return ret;
}

//----------------------------------------------------------------------------------------

void WaterMaterialGenerator::generateFragmentProgramSource(Ogre::StringUtil::StrStreamType& outStream)
{	
	mTexCoord_i = 5;
	
	if (needShadows())
		fpRealtimeShadowHelperSource(outStream);
	
	outStream <<
	"struct PIn \n"
	"{	float4 p : POSITION;	float3 uv : TEXCOORD0;	float4 wp : TEXCOORD1; \n"
	"	float4 n : TEXCOORD2;	float4 t  : TEXCOORD3;	float4 b  : TEXCOORD4; \n" // projective texcoords packed in (n.w, t.w, b.w)
	"}; \n"
	"float4 main_fp(PIn IN,  \n";      ///  _water_
	
	outStream <<
	"	uniform sampler2D normalMap : TEXUNIT"+toStr(mNormalTexUnit)+", \n";
	if (needTerrainLightMap())
	{	
		outStream <<
		"	uniform sampler2D terrainLightMap : TEXUNIT"+toStr(mTerrainLightTexUnit)+", \n"
		"	uniform float terrainWorldSize, \n";
	}
	outStream <<
	"	uniform sampler2D depthMap : TEXUNIT"+toStr(mWaterDepthUnit)+", \n";
	
	fpShadowingParams(outStream);
	
	if (mParent->getRefract() || mParent->getReflect()) outStream <<
		"	uniform float inverseProjection, \n"; 
	
#ifndef ROAD_EDITOR
	if (mParent->getRefract() || mParent->pApp->NeedMRTBuffer())
#else
	if (mParent->getRefract())
#endif
		outStream <<
		"	uniform sampler2D refractionMap : TEXUNIT"+toStr(mScreenRefrUnit)+", \n";
	
	if (MaterialFactory::getSingleton().getReflect()) outStream <<
		"	uniform sampler2D reflectionMap : TEXUNIT"+toStr(mScreenReflUnit)+", \n";
	/*else*/ outStream <<
		"	uniform sampler2D skyMap : TEXUNIT"+toStr(mEnvTexUnit)+", \n";
	
	
	outStream <<
	"	uniform float3 ambient, \n" 
	"	uniform float3 lightDiff, \n"
	"   uniform float3 lightSpec0, \n"
	"	uniform float4 matSpec,	\n"
	//	fog
	"	uniform float3 fogColor, \n"
	//	light dir
	"	uniform float4 lightPos0,  uniform float3 camPos,  uniform float4x4 iTWMat, \n"
	//	--------  water specific  --------
	//	normal animation
	//	waveBump - smooth/noisy surface  Speed - movement scale in time
	//	HighFreq - small ripples/big flow ratio  Spec - specular multipler
	"	uniform float4 waveBump_Speed_HighFreq_Spec, \n"
	"	uniform float time, \n"

	//	depthMap has alpha value in .r (short depth range) for smooth terrain water transition,
	//	and .g has depth amout (big range) for deep water
	//	depthPars x - depthColor effect for shallow/deep (top view)  z - as x but for reflected (low angle)
	//	y - adding color alpha for deep water
	"	uniform float3 depthPars, \n"
	//	color for deep water, from waterDepth tex
	"	uniform float4 depthColor, \n"
	//	invTerSize scale to get depthMap uv from world pos (const for all, set at scene load)
	"	uniform float invTerSize, \n"

	//	deep (far), shallow (near), from fresnel (has nothing to do with depth)
	"	uniform float4 deepColor, \n"
	"	uniform float4 shallowColor, \n"
	"	uniform float4 reflectionColor, \n"
	//	fresnel power and bias
	"	uniform float2 fresnelPowerBias, \n";

#ifndef ROAD_EDITOR
if (mParent->pApp->NeedMRTBuffer())
{
#endif
	outStream <<
	"	uniform float far, \n";
#ifndef ROAD_EDITOR
}
#endif

	outStream <<

	//	reflVal - reflClr to waterClr amout
	//	Refl2 - 2d screen reflection amout (lerp to 3d)
	//	Distort - amount of noise for refraction
	//	Opacity - 2d screen refraction opacity / transparency
	"	uniform float4 reflVal_Refl2_Distort_Opacity \n"

	"): COLOR0 \n"
	"{ \n";

	if (mParent->getRefract() || mParent->getReflect())
	{
		outStream <<
		"	float4 projCoord = float4(IN.n.w, IN.t.w, 1, IN.b.w); \n"
		"	float2 projUV = projCoord.xy / projCoord.w; \n"
		"	if (inverseProjection == -1)  projUV.y = 1-projUV.y; \n";
	}
		
	if (needTerrainLightMap()) outStream <<
		"	float4 wsNormal = float4(1.0, 1.0, 1.0, IN.wp.x); \n"
		"	float enableTerrainLightMap = 1.0; \n";
	if (needTerrainLightMap() || needShadows()) outStream <<
		"	float4 texCoord = float4(1.0, 1.0, IN.uv.z, IN.wp.z); \n";
	
	fpCalcShadowSource(outStream);
	
	outStream <<
	"	float t = waveBump_Speed_HighFreq_Spec.y * time; \n"
		//  waves  sum 4 normal tex
	"	float2 uv1 = float2(-sin(-0.06f * t),cos(-0.06f * t)) * 0.5f; \n"  // slow big waves
	"	float2 uv2 = float2( cos( 0.062f* t),sin( 0.062f* t)) * 0.4f; \n"
	"	float2 uw1 = float2(-sin(-0.11f * t),cos(-0.11f * t)) * 0.23f; \n"  // fast small waves
	"	float2 uw2 = float2( cos( 0.112f* t),sin( 0.112f* t)) * 0.21f; \n"
	"	const float w1 = 0.25 + waveBump_Speed_HighFreq_Spec.z, w2 = 0.25 - waveBump_Speed_HighFreq_Spec.z; \n"
	"	float3 normalTex = ( \n"
	"		(tex2D(normalMap, IN.uv.xy*8 - uw2).rgb * 2.0 - 1.0) * w1 + \n"
	"		(tex2D(normalMap, IN.uv.xy*8 - uw1).rgb * 2.0 - 1.0) * w1 + \n"
	"		(tex2D(normalMap, IN.uv.xy*2 + uv1).rgb * 2.0 - 1.0) * w2 + \n"
	"		(tex2D(normalMap, IN.uv.xy*2 + uv2).rgb * 2.0 - 1.0) * w2); \n"
	"	normalTex = lerp(normalTex, float3(0,0,1), waveBump_Speed_HighFreq_Spec.x); \n";

	if (mParent->getReflect() || mParent->getRefract()) outStream <<
		"	float2 projUV_refra = projUV; \n"
		"	projUV += normalTex.yx * reflVal_Refl2_Distort_Opacity.z; \n"
		"	projUV_refra += normalTex.yx * max(0, reflVal_Refl2_Distort_Opacity.z); \n";

#ifndef ROAD_EDITOR
if (!mParent->pApp->NeedMRTBuffer())
{
#endif
	//  depthMap for alpha near terrain and deep water
	outStream <<
	"	float2 uva = float2(IN.wp.z * -invTerSize + 0.5f, IN.wp.x * invTerSize + 0.5f); \n"
	"	float2 aa = tex2D(depthMap, uva).rg; \n"
	"	discard(aa.r < 0.01f); \n";  // this is water inside terrain no need to render
#ifndef ROAD_EDITOR
}
else
{
	outStream << "float2 aa = float2(1,1); \n";
	outStream <<
	"	float screendepth = tex2D(depthMap, projUV_refra).r; \n"
	"	aa.g = (screendepth * far - IN.uv.z) / 100.f; \n"
	"	aa.r = saturate(aa.g * 50); \n"
	"	aa.g = saturate(aa.g); \n";
}
#endif

outStream <<

		//  normal to object space
	"	float3x3 tbn = float3x3(IN.t.xyz, IN.b.xyz, IN.n.xyz); \n"
	"	float3 normal = mul(transpose(tbn), normalTex.xyz); \n"
	"	normal = normalize(mul((float3x3)iTWMat, normal)); \n"

		//  diffuse, specular
	"	float3 ldir = normalize(lightPos0.xyz - (lightPos0.w * IN.wp.xyz)); \n"
	"	float3 camDir = normalize(camPos - IN.wp.xyz); \n"
	"	float3 halfVec = normalize(ldir + camDir); \n"
	"	float3 specular = pow(max(dot(normal, halfVec), 0), matSpec.w); \n";
	
	if (needShadows() || needTerrainLightMap())  outStream <<
		"	float3 diffC = lightDiff.xyz * max(0.5, shadowing); \n"
		"	float3 specC = specular * waveBump_Speed_HighFreq_Spec.w * lightSpec0 * matSpec.rgb * shadowing; \n";
	else outStream <<
		"	float3 diffC = lightDiff.xyz; \n"
		"	float3 specC = specular * waveBump_Speed_HighFreq_Spec.w * lightSpec0 * matSpec.rgb; \n";
	
#ifndef ROAD_EDITOR
	if (mParent->getRefract() || mParent->pApp->NeedMRTBuffer())
#else
	if (mParent->getRefract())
#endif
		outStream <<
		"	float4 refraction = tex2D(refractionMap, projUV_refra); \n";
	
	outStream <<
		//  reflection  3D vec to sky dome map 2D uv
	"	float3 refl = reflect(-camDir, normal); \n"
	"	const float PI = 3.1415926536f; \n"
	"	float2 refl2; \n"
	"	refl2.x = /*(refl.x == 0) ? 0 :*/ ( (refl.z < 0.0) ? atan2(-refl.z, refl.x) : (2*PI - atan2(refl.z, refl.x)) ); \n"
	"	refl2.x = 1 - refl2.x / (2*PI); \n"  // yaw 0..1
	"	refl2.y = 1 - asin(refl.y) / PI*2; \n" //pitch 0..1
	"	float4 reflection = tex2D(skyMap, refl2) * reflectionColor; \n";

	if (mParent->getReflect())
		//	2d screen reflection is lerp-ed with 3d sky reflection
		outStream <<
		"	float4 reflection2D = tex2D(reflectionMap, projUV) * reflectionColor * 1.3f; \n"  //par 1.3f
		"	reflection = lerp(reflection, reflection2D, reflVal_Refl2_Distort_Opacity.y); \n";

	outStream <<
		//  fresnel
	"	float facing = 1.0 - max(abs(dot(camDir, normal)), 0); \n"
	"	float fresnel = saturate(fresnelPowerBias.y + pow(facing, fresnelPowerBias.x)); \n"
		//  water color
	"	float4 waterClr = lerp( lerp(shallowColor, deepColor, facing), depthColor, aa.g * depthPars.x); \n"
	"	waterClr.xyz = ambient * waterClr.xyz + diffC * waterClr.xyz; \n"
	"	float4 reflClr  = lerp( lerp(waterClr, reflection, fresnel),   depthColor, aa.g * depthPars.z); \n"
	"	float3 clr = /*diffC */ (specC + lerp( reflClr.rgb, waterClr.rgb, reflVal_Refl2_Distort_Opacity.x) ); \n";
	
	
	// fog, we apply it before adding refraction, since the refracted part already has fog from the RTT
	outStream <<
	"	clr = lerp(clr, fogColor, /*IN.fogVal*/IN.wp.w ); \n";
	
	outStream <<
	"	float alpha = saturate(aa.g * depthPars.y + aa.r * (waterClr.a + specC.r)); \n";

#ifndef ROAD_EDITOR
	if (mParent->getRefract() || mParent->pApp->NeedMRTBuffer())
#else
	if (mParent->getRefract())
#endif
		outStream << 
		"	clr = lerp(refraction, clr, alpha * reflVal_Refl2_Distort_Opacity.w); \n";
		
	outStream <<
		"	return float4(clr, alpha); \n";
		//"	return float4(aa.g, aa.g, aa.g, 1); \n";
		//"	return float4(realdepth); \n";
	
	outStream <<
	"} \n";
	
	//LogO("\n"+outStream.str());
}

//----------------------------------------------------------------------------------------

void WaterMaterialGenerator::fragmentProgramParams(Ogre::HighLevelGpuProgramPtr program)
{
	GpuProgramParametersSharedPtr params = program->getDefaultParameters();
	
	params->setIgnoreMissingParams(true);

	params->setNamedAutoConstant("iTWMat", GpuProgramParameters::ACT_INVERSE_TRANSPOSE_WORLD_MATRIX);
	params->setNamedAutoConstant("fogColor", GpuProgramParameters::ACT_FOG_COLOUR);
	params->setNamedAutoConstant("lightSpec0", GpuProgramParameters::ACT_LIGHT_SPECULAR_COLOUR, 0);
	params->setNamedAutoConstant("lightPos0", GpuProgramParameters::ACT_LIGHT_POSITION, 0);
	params->setNamedAutoConstant("camPos", GpuProgramParameters::ACT_CAMERA_POSITION);
	params->setNamedAutoConstant("time", GpuProgramParameters::ACT_TIME);
	params->setNamedAutoConstant("ambient", GpuProgramParameters::ACT_AMBIENT_LIGHT_COLOUR);
	params->setNamedAutoConstant("lightDiff", GpuProgramParameters::ACT_LIGHT_DIFFUSE_COLOUR, 0);
	params->setNamedAutoConstant("inverseProjection", GpuProgramParameters::ACT_RENDER_TARGET_FLIPPING);
	params->setNamedAutoConstant("far", GpuProgramParameters::ACT_FAR_CLIP_DISTANCE);
	params->setNamedConstantFromTime("time", 1);
	individualFragmentProgramParams(params);
}

//----------------------------------------------------------------------------------------

void WaterMaterialGenerator::individualFragmentProgramParams(Ogre::GpuProgramParametersSharedPtr params)
{
	// can't set a vector2, need to pack it in vector3
	#define _vec3(prop) Vector3(prop.x, prop.y, 1.0)
	
	params->setIgnoreMissingParams(true);
	
	params->setNamedConstant("enableShadows", Real(1.f));
	params->setNamedConstant("waveBump_Speed_HighFreq_Spec", mDef->mProps->waveBump_Speed_HighFreq_Spec);
	params->setNamedConstant("depthPars", mDef->mProps->depthPars);
	params->setNamedConstant("depthColor", mDef->mProps->depthColour);
	params->setNamedConstant("deepColor", mDef->mProps->deepColour);
	params->setNamedConstant("shallowColor", mDef->mProps->shallowColour);
	params->setNamedConstant("reflectionColor", mDef->mProps->reflectionColour);
	params->setNamedConstant("matSpec", mDef->mProps->specular);
	params->setNamedConstant("reflVal_Refl2_Distort_Opacity", mDef->mProps->reflVal_Refl2_Distort_Opacity);
	params->setNamedConstant("fresnelPowerBias", Vector3(mDef->mProps->fresnelPower, mDef->mProps->fresnelBias, 0));
	
	if (needShadows())
	{
		params->setNamedConstant("pssmSplitPoints", mParent->pApp->splitPoints);
		for (int i=0; i<mParent->getNumShadowTex(); ++i)
			params->setNamedAutoConstant("invShadowMapSize"+toStr(i), GpuProgramParameters::ACT_INVERSE_TEXTURE_SIZE, i+mShadowTexUnit_start);
	}
}

//----------------------------------------------------------------------------------------

HighLevelGpuProgramPtr WaterMaterialGenerator::createFragmentProgram()
{
	HighLevelGpuProgramManager& mgr = HighLevelGpuProgramManager::getSingleton();
	std::string progName = mDef->getName() + "_FP";

	HighLevelGpuProgramPtr ret = mgr.getByName(progName);
	if (!ret.isNull())
		mgr.remove(progName);

	ret = mgr.createProgram(progName, ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME, 
			"cg", GPT_FRAGMENT_PROGRAM);

	if(MRTSupported())
		ret->setParameter("profiles", "ps_4_0 ps_3_0 fp40");
	else
		ret->setParameter("profiles", "ps_4_0 ps_3_0 fp40 arbfp1");	

	ret->setParameter("entry_point", "main_fp");

	StringUtil::StrStreamType outStream;
	generateFragmentProgramSource(outStream);
	ret->setSource(outStream.str());
	ret->load();

	GpuProgramParametersSharedPtr params = ret->getDefaultParameters();
	fragmentProgramParams(ret);
	
	return ret;
}
