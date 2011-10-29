#include "pch.h"
#include "../Defines.h"

#include "MaterialProperties.h"
#include "MaterialGenerator.h"
#include "MaterialDefinition.h"
#include "MaterialFactory.h"
#include "ShaderProperties.h"

#ifndef ROAD_EDITOR
	#include "../OgreGame.h"
#else
	#include "../../editor/OgreApp.h"
#endif

#include <OgreMaterialManager.h>
#include <OgreTechnique.h>
#include <OgrePass.h>
#include <OgreTextureUnitState.h>
#include <OgreHighLevelGpuProgramManager.h>
#include <OgreHighLevelGpuProgram.h>
#include <OgreGpuProgramParams.h>
using namespace Ogre;

void MaterialGenerator::generate(bool fixedFunction)
{	
	MaterialPtr mat = prepareMaterial(mDef->getName());
	
	// reset some attributes
	mDiffuseTexUnit = 0; mNormalTexUnit = 0; mEnvTexUnit = 0; mAlphaTexUnit = 0;
	mShadowTexUnit_start = 0; mTexUnit_i = 0;
	
	std::string diffuseMap = pickTexture(&mDef->mProps->diffuseMaps);
	std::string normalMap = pickTexture(&mDef->mProps->normalMaps);
	std::string alphaMap = pickTexture(&mDef->mProps->alphaMaps);
	
	Ogre::Technique* technique = mat->createTechnique();
	
	if (mDef->mProps->twoPass)
	{
		// create an ambient-only pass first
		Ogre::Pass* ambientPass = technique->createPass();
		ambientPass->setAmbient( mDef->mProps->ambient.x, mDef->mProps->ambient.y, mDef->mProps->ambient.z );
		ambientPass->setDiffuse( mDef->mProps->diffuse.x, mDef->mProps->diffuse.y, mDef->mProps->diffuse.z, 1.0 );
		
		ambientPass->setSpecular(mDef->mProps->specular.x, mDef->mProps->specular.y, mDef->mProps->specular.z, mDef->mProps->specular.w);
		
		ambientPass->setSceneBlending(SBT_TRANSPARENT_ALPHA);
		ambientPass->setDepthBias( mDef->mProps->depthBias );
		ambientPass->setDepthWriteEnabled(false);
		ambientPass->setCullingMode( chooseCullingModeAmbient() );
		
		Ogre::TextureUnitState* tu = ambientPass->createTextureUnitState( diffuseMap );
		tu->setName("diffuseMap");
		tu->setTextureAddressingMode(mDef->mProps->textureAddressMode);
		
		// create shaders
		HighLevelGpuProgramPtr fragmentProg, vertexProg;
		try
		{
			vertexProg = createAmbientVertexProgram();
			fragmentProg = createAmbientFragmentProgram();
		}
		catch (Ogre::Exception& e) {
			LogO(e.getFullDescription());
		}
		
		if (fragmentProg.isNull() || vertexProg.isNull() || 
			!fragmentProg->isSupported() || !vertexProg->isSupported())
		{
			LogO("[MaterialFactory] WARNING: ambient shader for material '" + mDef->getName()
				+ "' is not supported.");
		}
		else
		{
			ambientPass->setVertexProgram(vertexProg->getName());
			ambientPass->setFragmentProgram(fragmentProg->getName());
		}

		
	}

	//add ssao technique
	Technique* ssaopasstech = mat->createTechnique();
	ssaopasstech->setName("geom");
	ssaopasstech->setSchemeName("geom");
	// Only supporting one pass
	Pass* ssaopass = ssaopasstech->createPass();

	HighLevelGpuProgramManager& hmgr = HighLevelGpuProgramManager::getSingleton();
	HighLevelGpuProgramPtr vprog = hmgr.getByName("geom_vs");
	HighLevelGpuProgramPtr fprog = hmgr.getByName("geom_ps");
	ssaopass->setVertexProgram(vprog->getName());
	ssaopass->setFragmentProgram(fprog->getName());
	
	Ogre::Pass* pass = technique->createPass();
	
	if (!mDef->mProps->twoPass)
		pass->setAmbient( mDef->mProps->ambient.x, mDef->mProps->ambient.y, mDef->mProps->ambient.z );
	else
	{
		// already have ambient in first pass
		pass->setAmbient(0.0, 0.0, 0.0);
	}
	
	pass->setDiffuse( mDef->mProps->diffuse.x, mDef->mProps->diffuse.y, mDef->mProps->diffuse.z, 1.0 );
	
	if (!needShaders() || fixedFunction)
	{
		pass->setSpecular(mDef->mProps->specular.x, mDef->mProps->specular.y, mDef->mProps->specular.z, 1.0 );
		pass->setShininess(mDef->mProps->specular.w);
	}
	else
	{
		// shader assumes shininess in specular w component
		pass->setSpecular(mDef->mProps->specular.x, mDef->mProps->specular.y, mDef->mProps->specular.z, mDef->mProps->specular.w);
	}
	
	pass->setCullingMode(chooseCullingMode());
	
	if (!mDef->mProps->fog)
		pass->setFog(true); // actually this disables fog
		
	if (!mDef->mProps->lighting)
		pass->setLightingEnabled(false);
	
	if (mDef->mProps->sceneBlend == SBM_ALPHA_BLEND)
		pass->setSceneBlending(SBT_TRANSPARENT_ALPHA);
	else if (mDef->mProps->sceneBlend == SBM_COLOUR_BLEND)
		pass->setSceneBlending(SBT_TRANSPARENT_COLOUR);
	else if (mDef->mProps->sceneBlend == SBM_ADD)
		pass->setSceneBlending(SBT_ADD);
	else if (mDef->mProps->sceneBlend == SBM_MODULATE)
		pass->setSceneBlending(SBT_MODULATE);
		
	pass->setDepthWriteEnabled( mDef->mProps->depthWrite );
		
	pass->setDepthCheckEnabled( mDef->mProps->depthCheck );
		
	pass->setTransparentSortingEnabled( mDef->mProps->transparentSorting );
	
	pass->setAlphaRejectFunction( mDef->mProps->alphaRejectFunc );
	pass->setAlphaRejectValue( mDef->mProps->alphaRejectValue );
	
	if (mDef->mProps->depthBias != 0.f)
		pass->setDepthBias( mDef->mProps->depthBias );
	
	if (!needShaders() || fixedFunction)
	{		
		Ogre::TextureUnitState* tu;
		
		//!todo alpha map
		/// no idea how to blend this
		// alpha map
		/*if (needAlphaMap())
		{
			tu = pass->createTextureUnitState( alphaMap );
			//tu->setAlphaOperation(LBX_SOURCE1, LBS_CURRENT, LBS_TEXTURE);
			//tu->setColourOperation(LBO_ALPHA_BLEND);
		}*/
		
		// diffuse map
		if (needDiffuseMap())
		{
			tu = pass->createTextureUnitState( diffuseMap );
			tu->setTextureAddressingMode(mDef->mProps->textureAddressMode);
		}
		
		if (needEnvMap())
		{
			// env map
			tu = pass->createTextureUnitState();
			tu->setCubicTextureName( mDef->mProps->envMap, true );
			tu->setEnvironmentMap(true, TextureUnitState::ENV_REFLECTION);
			tu->setTextureAddressingMode(TextureUnitState::TAM_CLAMP);
			
			// blend with diffuse map using 'reflection amount' property
			tu->setColourOperationEx(LBX_BLEND_MANUAL, LBS_CURRENT, LBS_TEXTURE, 
									ColourValue::White, ColourValue::White, 1-mDef->mProps->reflAmount);
		}
	}
	else
	{
		Ogre::TextureUnitState* tu;
		// diffuse map
		if (needDiffuseMap())
		{
			tu = pass->createTextureUnitState( diffuseMap );
			tu->setName("diffuseMap");
			tu->setTextureAddressingMode(mDef->mProps->textureAddressMode);
			mDiffuseTexUnit = mTexUnit_i; mTexUnit_i++;
		}
		
		// alpha map
		if (needAlphaMap())
		{
			tu = pass->createTextureUnitState( alphaMap );
			tu->setName("alphaMap");
			mAlphaTexUnit = mTexUnit_i; mTexUnit_i++;
		}
		
		// env map
		if (needEnvMap())
		{
			tu = pass->createTextureUnitState( mDef->mProps->envMap );
			tu->setName("envMap");
			tu->setTextureAddressingMode(TextureUnitState::TAM_CLAMP);
			mEnvTexUnit = mTexUnit_i; mTexUnit_i++;
		}
		
		// normal map
		if (needNormalMap())
		{
			tu = pass->createTextureUnitState( normalMap );
			tu->setName("normalMap");
			mNormalTexUnit = mTexUnit_i; mTexUnit_i++;
		}
		
		// shadow maps
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
		
		// create shaders		
		if (!mShaderCached)
		{
			try
			{
				mVertexProgram = createVertexProgram();
				mFragmentProgram = createFragmentProgram();
			}
			catch (Ogre::Exception& e) {
				LogO(e.getFullDescription());
			}
			
			if (mFragmentProgram.isNull() || mVertexProgram.isNull() || 
				!mFragmentProgram->isSupported() || !mVertexProgram->isSupported())
			{
				LogO("[MaterialFactory] WARNING: shader for material '" + mDef->getName()
					+ "' is not supported, falling back to fixed-function");
				LogO("[MaterialFactory] Vertex program source: ");
				StringUtil::StrStreamType vSourceStr;
				generateVertexProgramSource(vSourceStr);
				LogO(vSourceStr.str());
				LogO("[MaterialFactory] Fragment program source: ");
				StringUtil::StrStreamType fSourceStr;
				generateFragmentProgramSource(fSourceStr);
				LogO(fSourceStr.str());
				
				mVertexProgram.setNull(); mFragmentProgram.setNull();
				generate(true);
				return;
			}
		}
		
		pass->setVertexProgram(mVertexProgram->getName());
		pass->setFragmentProgram(mFragmentProgram->getName());
		
		if (mShaderCached)
		{
			// set individual material shader params
			individualVertexProgramParams(pass->getVertexProgramParameters());
			individualFragmentProgramParams(pass->getFragmentProgramParameters());
		}
	}
	
	if (needShadows())
		mParent->splitMtrs.push_back( mDef->getName() );
}

//----------------------------------------------------------------------------------------

MaterialPtr MaterialGenerator::prepareMaterial(const std::string& name)
{
	MaterialPtr mat;
	if (MaterialManager::getSingleton().resourceExists(name))
	{
		mat = MaterialManager::getSingleton().getByName(name);
		mat->removeAllTechniques();
	}
	else
	{
		mat = MaterialManager::getSingleton().create(name, ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME);
		mat->removeAllTechniques(); // WTF ? ogre by default inserts a technique and a pass. why?
	}
	return mat;
}

//----------------------------------------------------------------------------------------

inline bool MaterialGenerator::needShaders()
{
	return mParent->getShaders() && mDef->mProps->shaders;
}

inline bool MaterialGenerator::needShadows()
{
	return mShader->shadows;
}

inline bool MaterialGenerator::needNormalMap()
{
	return mShader->normalMap;
}

inline bool MaterialGenerator::needEnvMap()
{
	return mShader->envMap;
}

inline bool MaterialGenerator::needDiffuseMap()
{
	return mShader->diffuseMap;
}

inline bool MaterialGenerator::needLightingAlpha()
{
	return mShader->lightingAlpha;
}

inline bool MaterialGenerator::needAlphaMap()
{
	return mShader->alphaMap;
}

inline bool MaterialGenerator::needFresnel()
{
	return mShader->fresnel;
}

inline bool MaterialGenerator::fpNeedLighting()
{
	return mShader->lighting;
}

inline bool MaterialGenerator::fpNeedWsNormal()
{
	return needEnvMap() || needNormalMap() || fpNeedLighting();
}

inline bool MaterialGenerator::fpNeedEyeVector()
{
	return needEnvMap() || fpNeedLighting();
}

inline bool MaterialGenerator::vpNeedTangent()
{
	return needNormalMap();
}

inline bool MaterialGenerator::vpNeedWMat()
{
	return fpNeedEyeVector();
}

inline bool MaterialGenerator::vpNeedWITMat()
{
	return fpNeedWsNormal();
}

inline bool MaterialGenerator::fpNeedTangentToCube()
{
	return (needNormalMap() || fpNeedEyeVector());
}

std::string MaterialGenerator::getChannel(unsigned int n)
{
	if (n == 0) 		return "x";
	else if (n == 1) 	return "y";
	else if (n == 2)	return "z";
	else 				return "w";
}

//----------------------------------------------------------------------------------------

std::string MaterialGenerator::pickTexture(textureMap* textures)
{
	if (textures->size() == 0) return "";
	
	// we assume the textures are sorted by size
	textureMap::iterator it;
	for (it = textures->begin(); it != textures->end(); ++it)
	{
		if ( it->first < mParent->getTexSize() ) continue;
		/* else */ break;
	}
	
	if (it == textures->end()) --it;
	
	return it->second;
}


//----------------------------------------------------------------------------------------

Ogre::CullingMode MaterialGenerator::chooseCullingMode()
{

	if 		(mDef->mProps->cullHardware == CULL_HW_NONE)
		return Ogre::CULL_NONE;
	else if (mDef->mProps->cullHardware == CULL_HW_CLOCKWISE)
		return Ogre::CULL_CLOCKWISE;
	else if (mDef->mProps->cullHardware == CULL_HW_ANTICLOCKWISE)
		return Ogre::CULL_ANTICLOCKWISE;
	else if (mDef->mProps->cullHardware == CULL_HW_CLOCKWISE_OR_NONE)
	{
		if (mParent->getShadowsDepth())
			return Ogre::CULL_NONE;
		else
			return Ogre::CULL_CLOCKWISE;
	}
	else if (mDef->mProps->cullHardware == CULL_HW_ANTICLOCKWISE_OR_NONE)
	{
		if (mParent->getShadowsDepth())
			return Ogre::CULL_NONE;
		else
			return Ogre::CULL_ANTICLOCKWISE;
	}
	return Ogre::CULL_NONE;
}

//----------------------------------------------------------------------------------------

Ogre::CullingMode MaterialGenerator::chooseCullingModeAmbient()
{

	if 		(mDef->mProps->cullHardwareAmbient == CULL_HW_NONE)
		return Ogre::CULL_NONE;
	else if (mDef->mProps->cullHardwareAmbient == CULL_HW_CLOCKWISE)
		return Ogre::CULL_CLOCKWISE;
	else if (mDef->mProps->cullHardwareAmbient == CULL_HW_ANTICLOCKWISE)
		return Ogre::CULL_ANTICLOCKWISE;
	else if (mDef->mProps->cullHardwareAmbient == CULL_HW_CLOCKWISE_OR_NONE)
	{
		if (mParent->getShadowsDepth())
			return Ogre::CULL_NONE;
		else
			return Ogre::CULL_CLOCKWISE;
	}
	else if (mDef->mProps->cullHardwareAmbient == CULL_HW_ANTICLOCKWISE_OR_NONE)
	{
		if (mParent->getShadowsDepth())
			return Ogre::CULL_NONE;
		else
			return Ogre::CULL_ANTICLOCKWISE;
	}
	return Ogre::CULL_NONE;
}

//----------------------------------------------------------------------------------------

HighLevelGpuProgramPtr MaterialGenerator::createVertexProgram()
{
	HighLevelGpuProgramManager& mgr = HighLevelGpuProgramManager::getSingleton();
	std::string progName = mDef->getName() + "_VP";

	HighLevelGpuProgramPtr ret = mgr.getByName(progName);
	if (!ret.isNull())
		mgr.remove(progName);

	ret = mgr.createProgram(progName, ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME, 
		"cg", GPT_VERTEX_PROGRAM);

	ret->setParameter("profiles", "vs_1_1 arbvp1");
	ret->setParameter("entry_point", "main_vp");

	StringUtil::StrStreamType sourceStr;
	generateVertexProgramSource(sourceStr);
	#ifdef SHADER_DEBUG
	LogO("Vertex program source for '"+mDef->getName()+"':\n");
	LogO(sourceStr.str());
	#endif
	ret->setSource(sourceStr.str());
	ret->load();
	vertexProgramParams(ret);
	
	return ret;
}

//----------------------------------------------------------------------------------------

void MaterialGenerator::generateVertexProgramSource(Ogre::StringUtil::StrStreamType& outStream)
{
	outStream << 
		"void main_vp( "
		"	float2 texCoord 					: TEXCOORD0, \n"
		"	float4 position 					: POSITION, \n";
	if (vpNeedTangent()) outStream <<
		"	float3 tangent						: TANGENT, \n";
	if (fpNeedWsNormal()) outStream <<
		"	float3 normal			 			: NORMAL, \n"
		"	out float3 oWsNormal  				: TEXCOORD1, \n";
	if (fpNeedEyeVector()) outStream <<
		"	uniform float4 eyePosition,	 \n";
	outStream <<
		"	out float4 oPosition			 	: POSITION, \n"
		"	out float4 objectPos				: COLOR, \n"; // running out of texcoords so putting this in COLOR since its unused.
	if (!needShadows()) outStream <<
		"	out float2 oTexCoord	 			: TEXCOORD0, \n";
	else outStream <<
		"	out float3 oTexCoord				: TEXCOORD0, \n";
		
	if (needNormalMap()) outStream <<
		"	uniform float bumpScale, \n";
		
	if (fpNeedTangentToCube()) outStream <<
		"	out	float4	oTangentToCubeSpace0	: TEXCOORD2, \n" // tangent to cube (world) space
		"	out	float4	oTangentToCubeSpace1	: TEXCOORD3, \n"
		"	out	float4	oTangentToCubeSpace2	: TEXCOORD4, \n";
	
	// fog
	if (mDef->mProps->fog) outStream <<
		"	uniform float4 fogParams, \n";
	
	if (needShadows()) {
		for (int i=0; i<mParent->getNumShadowTex(); ++i)
		{
			outStream << "out float4 oLightPosition"+toStr(i)+" : TEXCOORD"+toStr(i+5)+", \n";
		}
		outStream << "\n";
		for (int i=0; i<mParent->getNumShadowTex(); ++i)
		{
			outStream << "uniform float4x4 texWorldViewProjMatrix"+toStr(i)+", \n";
		}
		outStream << "\n";
	}

	if (vpNeedWITMat()) outStream <<
		"	uniform float4x4 wITMat, \n";
	if (vpNeedWMat()) outStream <<
		"	uniform float4x4 wMat, \n";
	outStream << 
	"	uniform float4x4 wvpMat \n"
	") \n"
	"{ \n"
	"	oPosition = mul(wvpMat, position); \n";
	
	if (fpNeedEyeVector()) outStream <<
		"	float3 eyeVector = mul( wMat, position ) - eyePosition; \n" // transform eye into view space
		"	oTangentToCubeSpace0.xyzw = eyeVector.xxxx; \n"
		"	oTangentToCubeSpace1.xyzw = eyeVector.yyyy; \n"
		"	oTangentToCubeSpace2.xyzw = eyeVector.zzzz; \n";

	if (needNormalMap()) outStream <<
	"	float3 binormal = cross(tangent, normal); \n"	// calculate binormal
	"	float3x3 tbn = float3x3( tangent * bumpScale, \n"			// build TBN
	"										binormal * bumpScale, \n"
	"										normal ); \n"
	"	float3x3 tbnInv = transpose(tbn); \n" // transpose TBN to get inverse rotation to obj space
	"	float3x3 tmp = mul( (float3x3) wITMat, tbnInv ); \n" // concatenate with the upper-left 3x3 of inverse transpose world matrix 
	"	oTangentToCubeSpace0.xyz = tmp[0]; \n" // store in tangentToCubeSpace to pass this on to fragment shader
	"	oTangentToCubeSpace1.xyz = tmp[1]; \n"
	"	oTangentToCubeSpace2.xyz = tmp[2]; \n";
		
	if (needShadows()) {
		 outStream <<
		"	oTexCoord.xy = texCoord; \n"
		"	oTexCoord.z = oPosition.z; \n";
		for (int i=0; i<mParent->getNumShadowTex(); ++i)
		{
			outStream << "oLightPosition"+toStr(i)+" = mul(texWorldViewProjMatrix"+toStr(i)+", position); \n";
		}
	}
	else outStream <<
		"	oTexCoord = texCoord; \n";
		
	outStream <<
	"	objectPos = position; \n";
	if (mDef->mProps->fog) outStream <<
		"	objectPos.w = saturate(fogParams.x * (oPosition.z - fogParams.y) * fogParams.w); \n"; // save fog amount in objectPos.w
	if (fpNeedWsNormal()) outStream <<
		"	oWsNormal = mul( (float3x3) wITMat, normal ); \n";
	outStream <<
	"} \n";
}

//----------------------------------------------------------------------------------------

void MaterialGenerator::vertexProgramParams(HighLevelGpuProgramPtr program)
{
	GpuProgramParametersSharedPtr params = program->getDefaultParameters();
	
	if (vpNeedWMat())
		params->setNamedAutoConstant("wMat", GpuProgramParameters::ACT_WORLD_MATRIX);
	params->setNamedAutoConstant("wvpMat", GpuProgramParameters::ACT_WORLDVIEWPROJ_MATRIX);
	if (fpNeedWsNormal())
		params->setNamedAutoConstant("wITMat", GpuProgramParameters::ACT_INVERSE_TRANSPOSE_WORLD_MATRIX);
	if (fpNeedEyeVector())
		params->setNamedAutoConstant("eyePosition", GpuProgramParameters::ACT_CAMERA_POSITION);
	
	if (mDef->mProps->fog)
		params->setNamedAutoConstant("fogParams", GpuProgramParameters::ACT_FOG_PARAMS);
		
	individualVertexProgramParams(params);
}

//----------------------------------------------------------------------------------------

void MaterialGenerator::individualVertexProgramParams(GpuProgramParametersSharedPtr params)
{
	if (needNormalMap())
		params->setNamedConstant("bumpScale", mDef->mProps->bumpScale);
	
	if (needShadows())
	for (int i=0; i<mParent->getNumShadowTex(); ++i)
	{
		params->setNamedAutoConstant("texWorldViewProjMatrix"+toStr(i), GpuProgramParameters::ACT_TEXTURE_WORLDVIEWPROJ_MATRIX, i);
	}
}

//----------------------------------------------------------------------------------------

HighLevelGpuProgramPtr MaterialGenerator::createFragmentProgram()
{
	HighLevelGpuProgramManager& mgr = HighLevelGpuProgramManager::getSingleton();
	std::string progName = mDef->getName() + "_FP";

	HighLevelGpuProgramPtr ret = mgr.getByName(progName);
	if (!ret.isNull())
		mgr.remove(progName);

	ret = mgr.createProgram(progName, ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME, 
			"cg", GPT_FRAGMENT_PROGRAM);

	ret->setParameter("profiles", "ps_2_x arbfp1");
	ret->setParameter("entry_point", "main_fp");

	StringUtil::StrStreamType sourceStr;
	generateFragmentProgramSource(sourceStr);
	#ifdef SHADER_DEBUG
	LogO("Fragment  program source for '"+mDef->getName()+"':\n");
	LogO(sourceStr.str());
	#endif
	ret->setSource(sourceStr.str());
	ret->load();
	fragmentProgramParams(ret);
	
	return ret;
}

//----------------------------------------------------------------------------------------

void MaterialGenerator::generateFragmentProgramSource(Ogre::StringUtil::StrStreamType& outStream)
{
	if (needShadows())
	{
		/// shadow helper functions
		// 2x2 pcf
		outStream <<
		"float shadowPCF(sampler2D shadowMap, float4 shadowMapPos, float2 offset) \n"
		"{ \n"
		"	shadowMapPos = shadowMapPos / shadowMapPos.w; \n"
		"	float2 uv = shadowMapPos.xy; \n"
		"	float3 o = float3(offset, -offset.x) * 0.3f; \n"

		"	float c =	(shadowMapPos.z <= tex2D(shadowMap, uv.xy - o.xy).r) ? 1 : 0; \n"
		"	c +=		(shadowMapPos.z <= tex2D(shadowMap, uv.xy + o.xy).r) ? 1 : 0; \n"
		"	c +=		(shadowMapPos.z <= tex2D(shadowMap, uv.xy + o.zy).r) ? 1 : 0; \n"
		"	c +=		(shadowMapPos.z <= tex2D(shadowMap, uv.xy - o.zy).r) ? 1 : 0; \n"
		"	return c / 4;  \n"
		"} \n";
		
		// pssm
		outStream <<
		"float calcPSSMShadow(";
		
		for (int i=0; i<mParent->getNumShadowTex(); ++i)
			outStream << "sampler2D shadowMap"+toStr(i)+",  \n";
		outStream << "\n";
		for (int i=0; i<mParent->getNumShadowTex(); ++i)
			outStream << "float4 lsPos"+toStr(i)+",  \n";
		outStream << "\n";
		for (int i=0; i<mParent->getNumShadowTex(); ++i)
			outStream << "float4 invShadowMapSize"+toStr(i)+",  \n";
		outStream << "\n";
		
		outStream <<
		"	float4 pssmSplitPoints, float camDepth \n"
		") \n"
		"{ \n"
		"	float shadow; \n";
		
		for (int i=0; i<mParent->getNumShadowTex(); ++i)
		{
			if (i==0)
				outStream << "if (camDepth <= pssmSplitPoints.y) \n";
			else if (i < mParent->getNumShadowTex()-1)
				outStream << "else if (camDepth <= pssmSplitPoints."+getChannel(i+1)+") \n";
			else
				outStream << "else \n";
				
			outStream <<
			"{ \n"
			"	shadow = shadowPCF(shadowMap"+toStr(i)+", lsPos"+toStr(i)+", invShadowMapSize"+toStr(i)+".xy); \n"
			"} \n";
		}
		
		outStream <<
		"	return shadow; \n"
		"} \n";
	}
	
	outStream <<
		"void main_fp(";
	if (!needShadows()) outStream <<
		"	in float2 texCoord : TEXCOORD0, \n";
	else outStream <<
		"	in float3 texCoord : TEXCOORD0, \n";
	outStream <<
		"	in float4 position : COLOR, \n";
	if (fpNeedWsNormal()) outStream <<
		"	in float3 wsNormal : TEXCOORD1, \n";
	if (fpNeedTangentToCube()) outStream <<
		"	in float4 tangentToCubeSpace0 : TEXCOORD2, \n"
		"	in float4 tangentToCubeSpace1 : TEXCOORD3, \n"
		"	in float4 tangentToCubeSpace2 : TEXCOORD4, \n";
		
	if (needFresnel()) outStream <<
		"	uniform float fresnelBias, \n"
		"	uniform float fresnelScale, \n"
		"	uniform float fresnelPower, \n";
		
	if (needDiffuseMap()) outStream <<
		"	uniform sampler2D diffuseMap : TEXUNIT"+toStr(mDiffuseTexUnit)+", \n";
		
	if (needAlphaMap()) outStream <<
		"	uniform sampler2D alphaMap	 : TEXUNIT"+toStr(mAlphaTexUnit)+", \n";
		
	if (needNormalMap()) outStream <<
		"	uniform sampler2D normalMap  : TEXUNIT"+toStr(mNormalTexUnit)+", \n";
		
	if (needEnvMap()) outStream << 
		"	uniform samplerCUBE envMap : TEXUNIT"+toStr(mEnvTexUnit)+", \n"
		"	uniform float reflAmount, \n";
		
	// lighting params
	// only 1 directional light is supported
	if (fpNeedLighting())
	{
		outStream <<
		// light
		"	uniform float3 lightDiffuse, \n"
		"	uniform float3 lightSpecular, \n"
		"	uniform float4 lightPosition, \n"
		// material
		"	uniform float3 matAmbient, \n"
		"	uniform float3 matDiffuse, \n"
		"	uniform float4 matSpecular, \n"; // shininess in w
	}
	if (mDef->mProps->fog) outStream <<
		"	uniform float3 fogColor, \n";
		
	if (mDef->mProps->transparent && !needAlphaMap())
		outStream <<
		"	uniform float4 lightingAlpha, \n";
	
	if (needShadows())
	{
		for (int i=0; i<mParent->getNumShadowTex(); ++i)
		{
			outStream <<
		"	uniform sampler2D shadowMap"+toStr(i)+" : TEXUNIT"+toStr(mShadowTexUnit_start+i)+", \n";
		}
		outStream << "\n";
		for (int i=0; i<mParent->getNumShadowTex(); ++i)
			outStream << "in float4 lightPosition"+toStr(i)+" : TEXCOORD"+toStr(i+5)+", \n";
		outStream << "\n";
		for (int i=0; i<mParent->getNumShadowTex(); ++i)
			outStream << "uniform float4 invShadowMapSize"+toStr(i)+", \n";
		outStream << "\n";
		outStream << 
		"	uniform float4 pssmSplitPoints, \n";
	}
	
	
	outStream << 

		"	out float4 oColor : COLOR \n"
		") \n"
		"{ \n";
		
	// calc shadowing
	if (needShadows()) {
		outStream <<
		"	float shadowing;"
		"	shadowing = calcPSSMShadow(";
	for (int i=0; i<mParent->getNumShadowTex(); ++i)
		outStream << "shadowMap"+toStr(i)+", ";
	for (int i=0; i<mParent->getNumShadowTex(); ++i)
		outStream << "lightPosition"+toStr(i)+", ";
	for (int i=0; i<mParent->getNumShadowTex(); ++i)
		outStream << "invShadowMapSize"+toStr(i)+", ";
		
		outStream <<
		"pssmSplitPoints, texCoord.z); \n";
		
	}
		
	  if (fpNeedEyeVector()) outStream <<
		"	float3 eyeVector = normalize(float3(tangentToCubeSpace0.w, tangentToCubeSpace1.w, tangentToCubeSpace2.w)); \n";
	if (fpNeedWsNormal())
	{
		outStream <<
		"	float3 normal;";
		if (needNormalMap()) outStream <<
			"	float4 tsNormal = (tex2D( normalMap, texCoord.xy) * 2.0 - 1.0 ); \n" // fetch tangent space normal and decompress from range-compressed
			"	normal.x = dot( tangentToCubeSpace0.xyz, tsNormal.xyz ); \n"
			"	normal.y = dot( tangentToCubeSpace1.xyz, tsNormal.xyz ); \n"
			"	normal.z = dot( tangentToCubeSpace2.xyz, tsNormal.xyz ); \n";
		else outStream <<
			"	normal = wsNormal; \n";
		
		outStream << 
		"	normal = normalize(normal); \n"; // normalize
	}
	
	// fetch diffuse texture
	if (needDiffuseMap()) outStream <<
		"	float4 diffuseTex = tex2D(diffuseMap, texCoord); \n";
	
	// calculate lighting (per-pixel)
	if (fpNeedLighting())
	{
		outStream <<	
		// Compute the diffuse term
		"	float3 lightDir = normalize(lightPosition.xyz - (position.xyz * lightPosition.w)); \n"
		"	float diffuseLight = max(dot(lightDir, normal), 0); \n";
		if (needDiffuseMap()) outStream <<
			"	float3 diffuse = matDiffuse.xyz * lightDiffuse.xyz * diffuseTex.xyz * diffuseLight; \n";
		else outStream <<
			"	float3 diffuse = matDiffuse.xyz * lightDiffuse.xyz * diffuseLight; \n";
		outStream <<
		// Compute the specular term
		"	float3 viewVec = -eyeVector; \n"
		"	float3 half = normalize(lightDir + viewVec); \n"
		"	float specularLight = pow(max(dot(normal, half), 0), matSpecular.w); \n"
		"	if (matSpecular.x == 0 && matSpecular.y == 0 && matSpecular.z == 0) specularLight = 0; \n"
		"	if (diffuseLight <= 0) specularLight = 0; \n"
		"	float3 specular = matSpecular.xyz * lightSpecular.xyz * specularLight; \n";

		// Compute the ambient term
		if (needDiffuseMap()) outStream <<
			"	float3 ambient = diffuseTex.xyz * matAmbient.xyz; \n";
		else outStream <<
			"	float3 ambient = matAmbient.xyz; \n";
		// Add all terms together (also with shadow)
		if (needShadows()) outStream <<
		"	float3 lightColour = ambient + diffuse*shadowing + specular*shadowing; \n";
		else outStream <<
		"	float3 lightColour = ambient + diffuse + specular; \n";
	}
	
	// cube reflection
	if (needEnvMap())
	{
		if (needFresnel()) outStream <<
			"	float facing = 1.0 - max(abs(dot(eyeVector, normal)), 0); \n"
			"	float reflectionFactor = saturate(fresnelBias + fresnelScale * pow(facing, fresnelPower)); \n";
		
			//"	float reflectionFactor = fresnelBias + fresnelScale * pow(1 + abs(dot(eyeVector, normal)), fresnelPower); \n";
		else outStream <<
			"	float reflectionFactor = reflAmount; \n";
		outStream << 
		"	float3 r = reflect( eyeVector, normal ); \n" // calculate reflection vector
		"	float4 envColor = texCUBE(envMap, r); \n"; // fetch cube map

		if (fpNeedLighting()) outStream <<
		"	float4 color1 = lerp(float4(lightColour,1), envColor, reflectionFactor); \n";
		else outStream <<
		"	float4 color1 = lerp(diffuseColour, envColor, reflectionFactor); \n";
	}
	else
	{
		if (fpNeedLighting()) outStream <<
		"	float4 color1 = float4(lightColour,1); \n";
		else outStream <<
		"	float4 color1 = diffuseTex; \n";
	}
	
	// add fog
	if (mDef->mProps->fog) outStream <<
		"	oColor = lerp(color1, float4(fogColor,1), position.w); \n";
	else outStream <<
		"	oColor = color1; \n";
		
	// alpha
	if (mDef->mProps->transparent)
	{
		if (needAlphaMap()) outStream <<
			"	float alpha = tex2D(alphaMap, texCoord).r; \n"; // use only r channel
		else if (needLightingAlpha())
		{
			if (mDef->mProps->lightingAlpha.w != 0 && needDiffuseMap()) outStream <<
				"	float alpha = lightingAlpha.x + lightingAlpha.y * diffuseLight + lightingAlpha.z * specularLight + (1-diffuseTex.r)*lightingAlpha.w; \n";
			else outStream <<
				"	float alpha = lightingAlpha.x + lightingAlpha.y * diffuseLight + lightingAlpha.z * specularLight; \n";
		}
		else
		{
			if (needDiffuseMap()) outStream <<
				"	float alpha = diffuseTex.a; \n";
			else {
				outStream <<
				"	float alpha = 1.0; \n"; // no way to get alpha value, we don't have diffuse tex and user didnt supply lightingAlpha
				LogO("[MaterialFactory] WARNING: Material '"+mDef->getName()+"' declared as transparent, but no way to get alpha value.");
			}
		}
		outStream << 
		"	oColor.w = alpha; \n";
	}
		
	outStream << 
		"} \n";
}

//----------------------------------------------------------------------------------------

void MaterialGenerator::fragmentProgramParams(HighLevelGpuProgramPtr program)
{
	GpuProgramParametersSharedPtr params = program->getDefaultParameters();

	if (fpNeedLighting())
	{
		params->setNamedAutoConstant("lightDiffuse", GpuProgramParameters::ACT_LIGHT_DIFFUSE_COLOUR, 0);
		params->setNamedAutoConstant("lightPosition", GpuProgramParameters::ACT_LIGHT_POSITION, 0);
		params->setNamedAutoConstant("matAmbient", GpuProgramParameters::ACT_SURFACE_AMBIENT_COLOUR);
		params->setNamedAutoConstant("matDiffuse", GpuProgramParameters::ACT_SURFACE_DIFFUSE_COLOUR);
		params->setNamedAutoConstant("matSpecular", GpuProgramParameters::ACT_SURFACE_SPECULAR_COLOUR);
	}
	
	if (mDef->mProps->fog)
		params->setNamedAutoConstant("fogColor", GpuProgramParameters::ACT_FOG_COLOUR);
	
	individualFragmentProgramParams(params);
}

//----------------------------------------------------------------------------------------

void MaterialGenerator::individualFragmentProgramParams(Ogre::GpuProgramParametersSharedPtr params)
{
	if (needEnvMap() && !needFresnel())
	{
		params->setNamedConstant("reflAmount", mDef->mProps->reflAmount);
	}
	if (needFresnel())
	{
		params->setNamedConstant("fresnelScale", mDef->mProps->fresnelScale);
		params->setNamedConstant("fresnelBias", mDef->mProps->fresnelBias);
		params->setNamedConstant("fresnelPower", mDef->mProps->fresnelPower);
	}

	if (needLightingAlpha())
		params->setNamedConstant("lightingAlpha", mDef->mProps->lightingAlpha);
		
	if (fpNeedLighting())
		params->setNamedConstant("lightSpecular", mDef->mProps->specular);
	
	if (needShadows())
	{
		params->setNamedConstant("pssmSplitPoints", mParent->pApp->splitPoints);
		for (int i=0; i<mParent->getNumShadowTex(); ++i)
			params->setNamedAutoConstant("invShadowMapSize"+toStr(i), GpuProgramParameters::ACT_INVERSE_TEXTURE_SIZE, i+mShadowTexUnit_start);
	}
}

//----------------------------------------------------------------------------------------

HighLevelGpuProgramPtr MaterialGenerator::createAmbientVertexProgram()
{
	HighLevelGpuProgramManager& mgr = HighLevelGpuProgramManager::getSingleton();
	std::string progName = mDef->getName() + "_ambient_VP";

	HighLevelGpuProgramPtr ret = mgr.getByName(progName);
	if (!ret.isNull())
		mgr.remove(progName);

	ret = mgr.createProgram(progName, ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME, 
		"cg", GPT_VERTEX_PROGRAM);

	ret->setParameter("profiles", "vs_1_1 arbvp1");
	ret->setParameter("entry_point", "main_vp");

	StringUtil::StrStreamType sourceStr;
	
	sourceStr <<
	"void main_vp( \n"
	"	in float2 uv, \n"
	"	in float4 position : POSITION, \n"
	"	uniform float4x4 wvpMat, \n"
	"	out float4 oPos : POSITION, out float2 oUV : TEXCOORD0) \n"
	"{ \n"
	"	oPos = mul(wvpMat, position);  oUV = uv; \n"
	"} \n";
	
	ret->setSource(sourceStr.str());
	ret->load();
	
	// params
	GpuProgramParametersSharedPtr params = ret->getDefaultParameters();
	params->setNamedAutoConstant("wvpMat", GpuProgramParameters::ACT_WORLDVIEWPROJ_MATRIX);
	
	return ret;
}

//----------------------------------------------------------------------------------------

HighLevelGpuProgramPtr MaterialGenerator::createAmbientFragmentProgram()
{
	HighLevelGpuProgramManager& mgr = HighLevelGpuProgramManager::getSingleton();
	std::string progName = mDef->getName() + "_ambient_FP";

	HighLevelGpuProgramPtr ret = mgr.getByName(progName);
	if (!ret.isNull())
		mgr.remove(progName);

	ret = mgr.createProgram(progName, ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME, 
		"cg", GPT_FRAGMENT_PROGRAM);

	ret->setParameter("profiles", "ps_2_0 arbfp1");
	ret->setParameter("entry_point", "main_fp");

	StringUtil::StrStreamType sourceStr;
	
	sourceStr <<
	"float4 main_fp(in float2 uv : TEXCOORD0, \n"
	"	uniform float3 ambient,  uniform float4 matDif, \n"
	"	uniform sampler2D diffuseMap): COLOR0 \n"
	"{ \n"
	"	float4 diffuseTex = tex2D(diffuseMap, uv); \n"
	"	return float4(ambient * matDif.rgb * diffuseTex.rgb, diffuseTex.a); \n"
	"} \n";
	
	ret->setSource(sourceStr.str());
	ret->load();
	
	// params
	GpuProgramParametersSharedPtr params = ret->getDefaultParameters();
	params->setNamedAutoConstant("ambient", GpuProgramParameters::ACT_SURFACE_AMBIENT_COLOUR );
	params->setNamedAutoConstant("matDif", GpuProgramParameters::ACT_SURFACE_DIFFUSE_COLOUR );
	
	return ret;
}