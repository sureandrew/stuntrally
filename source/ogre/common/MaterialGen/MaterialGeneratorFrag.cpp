#include "pch.h"
#include "../Defines.h"

#include <OgreMaterialManager.h>
#include <OgreTechnique.h>
#include <OgrePass.h>
#include <OgreTextureUnitState.h>
#include <OgreHighLevelGpuProgramManager.h>
#include <OgreHighLevelGpuProgram.h>
#include <OgreGpuProgramParams.h>
#include <OgreRoot.h>
using namespace Ogre;

#include "MaterialGenerator.h"
#include "MaterialDefinition.h"
#include "MaterialFactory.h"
#include "ShaderProperties.h"

#ifndef ROAD_EDITOR
	#include "../../OgreGame.h"
#else
	#include "../../../editor/OgreApp.h"
#endif


//  Fragment program
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

	if(MRTSupported())
	{
		ret->setParameter("profiles", "ps_4_0 ps_3_0 fp40");
	}
	else
	{
		ret->setParameter("profiles", "ps_4_0 ps_3_0 arbfp1");	
	}
	ret->setParameter("entry_point", "main_fp");

	StringUtil::StrStreamType sourceStr;
	generateFragmentProgramSource(sourceStr);
	ret->setSource(sourceStr.str());
	ret->load();
	fragmentProgramParams(ret);
	
	return ret;
}

//----------------------------------------------------------------------------------------

void MaterialGenerator::fpRealtimeShadowHelperSource(Ogre::StringUtil::StrStreamType& outStream)
{
	// use tex2Dlod on ps3 and up because its faster
	if (MRTSupported()) outStream <<
	"float TEX2DLOD(sampler2D map, float2 uv) \n"
	"{ \n"
	"	return tex2Dlod(map, float4(uv.xy, 0, 0)).r; \n"
	"} \n";
	else outStream <<
	"float TEX2DLOD(sampler2D map, float2 uv) \n"
	"{ \n"
	"	return tex2D(map, uv).r; \n"
	"} \n";
	
	outStream <<
	"float shadowPCF(sampler2D shadowMap, float4 shadowMapPos, float2 offset) \n"
	"{ \n"
	"	shadowMapPos = shadowMapPos / shadowMapPos.w; \n"
	"	float2 uv = shadowMapPos.xy; \n"
	//"	float3 o = float3(offset, -offset.x); \n";
	"float3 o = float3(0.0005, 0.0005, 0.0005); \n";
	
	String cmp1, cmp2;
	if (mParent->getShadowsDepth())
	{
		cmp1 = "(shadowMapPos.z <= ";
		cmp2 = ") ? 1 : 0";
	}
	else
	{
		cmp1 = "(1 > ";
		cmp2 = ") ? 0 : 1";
	}
	
	if (mParent->getShadowsFilterSize() == 2) outStream << // 2x2 offsets 0.5
		"	o *= 0.5; \n"
		"	float c =	"<<cmp1<<"TEX2DLOD(shadowMap,  uv.xy - o.xy)"<<cmp2<<"; \n"
		"	c +=		"<<cmp1<<"TEX2DLOD(shadowMap,  uv.xy + o.xy)"<<cmp2<<"; \n"
		"	c +=		"<<cmp1<<"TEX2DLOD(shadowMap,  uv.xy + o.zy)"<<cmp2<<"; \n"
		"	c +=		"<<cmp1<<"TEX2DLOD(shadowMap,  uv.xy - o.zy)"<<cmp2<<"; \n";
	else if (mParent->getShadowsFilterSize() == 3) outStream << // 3x3 offsets 1, 0
		"	float c =	"<<cmp1<<"TEX2DLOD(shadowMap,  uv.xy)"<<cmp2<<"; \n" // center
		"	c +=		"<<cmp1<<"TEX2DLOD(shadowMap,  uv.xy + o.xy)"<<cmp2<<"; \n" // bottom right
		"	c +=		"<<cmp1<<"TEX2DLOD(shadowMap,  uv.xy + float2(0, o.y))"<<cmp2<<"; \n" // bottom
		"	c +=		"<<cmp1<<"TEX2DLOD(shadowMap,  uv.xy + o.zy)"<<cmp2<<"; \n" // bottom left
		"	c +=		"<<cmp1<<"TEX2DLOD(shadowMap,  uv.xy + float2(o.z, 0))"<<cmp2<<"; \n" // left
		"	c +=		"<<cmp1<<"TEX2DLOD(shadowMap,  uv.xy + float2(o.x, 0))"<<cmp2<<"; \n" // right
		"	c +=		"<<cmp1<<"TEX2DLOD(shadowMap,  uv.xy - float2(0, o.y))"<<cmp2<<"; \n" // top
		"	c +=		"<<cmp1<<"TEX2DLOD(shadowMap,  uv.xy - o.xy)"<<cmp2<<"; \n" // top left
		"	c +=		"<<cmp1<<"TEX2DLOD(shadowMap,  uv.xy - o.zy)"<<cmp2<<"; \n"; // top right
	else if (mParent->getShadowsFilterSize() == 4) outStream << // 4x4 offsets 1.5, 0.5
		"	o *= 0.5; \n"
		"	float c =	"<<cmp1<<"TEX2DLOD(shadowMap,  uv.xy - 3*o.xy)"<<cmp2<<"; \n" // top left *2
		"	c +=	"<<cmp1<<"TEX2DLOD(shadowMap,  uv.xy - o.xy)"<<cmp2<<"; \n" // top left
		"	c +=	"<<cmp1<<"TEX2DLOD(shadowMap,  uv.xy - 3*o.zy)"<<cmp2<<"; \n" // top right *2
		"	c +=	"<<cmp1<<"TEX2DLOD(shadowMap,  uv.xy - o.zy)"<<cmp2<<"; \n" // top right
		"	c +=	"<<cmp1<<"TEX2DLOD(shadowMap,  uv.xy + 3*o.zy)"<<cmp2<<"; \n" // bottom left *2
		"	c +=	"<<cmp1<<"TEX2DLOD(shadowMap,  uv.xy + o.zy)"<<cmp2<<"; \n" // bottom left
		"	c +=	"<<cmp1<<"TEX2DLOD(shadowMap,  uv.xy + 3*o.xy)"<<cmp2<<"; \n" // bottom right *2
		"	c +=	"<<cmp1<<"TEX2DLOD(shadowMap,  uv.xy + o.xy)"<<cmp2<<"; \n" // bottom right
		"	c +=	"<<cmp1<<"TEX2DLOD(shadowMap,  uv.xy - float2(1,3)*o.xy)"<<cmp2<<"; \n" // top*2 left
		"	c +=	"<<cmp1<<"TEX2DLOD(shadowMap,  uv.xy - float2(1,3)*o.zy)"<<cmp2<<"; \n" // top*2 right
		"	c +=	"<<cmp1<<"TEX2DLOD(shadowMap,  uv.xy + float2(1,3)*o.zy)"<<cmp2<<"; \n" // bottom*2 left
		"	c +=	"<<cmp1<<"TEX2DLOD(shadowMap,  uv.xy + float2(1,3)*o.xy)"<<cmp2<<"; \n" // bottom*2 right
		"	c +=	"<<cmp1<<"TEX2DLOD(shadowMap,  uv.xy + float2(3,1)*o.zy)"<<cmp2<<"; \n" // left*2 bottom
		"	c +=	"<<cmp1<<"TEX2DLOD(shadowMap,  uv.xy - float2(3,1)*o.xy)"<<cmp2<<"; \n" // left*2 top
		"	c +=	"<<cmp1<<"TEX2DLOD(shadowMap,  uv.xy + float2(3,1)*o.xy)"<<cmp2<<"; \n" // right*2 bottom
		"	c +=	"<<cmp1<<"TEX2DLOD(shadowMap,  uv.xy - float2(3,1)*o.zy)"<<cmp2<<"; \n"; // right*2 top
	else outStream << // no filter
	"	float c = "<<cmp1<<"TEX2DLOD(shadowMap,  uv.xy)"<<cmp2<<"; \n";
	
	outStream <<
		"	return c / "<< mParent->getShadowsFilterSize() * mParent->getShadowsFilterSize() << ";  \n"
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
	
	switch (mParent->getNumShadowTex())
	{
	case 3:
		outStream <<
		"shadow = (camDepth <= pssmSplitPoints.y) ? \n"
		"	shadowPCF(shadowMap0, lsPos0, invShadowMapSize0.xy) : \n"
		"		( (camDepth <= pssmSplitPoints.z) ? \n"
		"	shadowPCF(shadowMap1, lsPos1, invShadowMapSize1.xy) : \n"
		"	shadowPCF(shadowMap2, lsPos2, invShadowMapSize2.xy) ); \n";  break;
	case 2:
		outStream <<
		"shadow = (camDepth <= pssmSplitPoints.y) ? \n"
		"	shadowPCF(shadowMap0, lsPos0, invShadowMapSize0.xy) : \n"
		"	shadowPCF(shadowMap1, lsPos1, invShadowMapSize1.xy); \n";  break;
	case 1:
		outStream <<
		"shadow = \n"
		"	shadowPCF(shadowMap0, lsPos0, invShadowMapSize0.xy) : \n";  break;
	case 0:
		"shadow = 0.f; \n";
	}

	/*for (int i=0; i<mParent->getNumShadowTex(); ++i)
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
	}/**/
	
	outStream <<
	"	return shadow; \n"
	"} \n";
}

//----------------------------------------------------------------------------------------

void MaterialGenerator::fpShadowingParams(Ogre::StringUtil::StrStreamType& outStream)
{
	if (needShadows())
	{
		for (int i=0; i<mParent->getNumShadowTex(); ++i)
		{
			outStream <<
		"	uniform sampler2D shadowMap"+toStr(i)+" : TEXUNIT"+toStr(mShadowTexUnit_start+i)+", \n";
		}
		outStream << "\n";
		for (int i=0; i<mParent->getNumShadowTex(); ++i)
			outStream << "in float4 lightPosition"+toStr(i)+" : TEXCOORD"+toStr(mTexCoord_i++)+", \n";
		outStream << "\n";
		outStream << 
		"	uniform float4 pssmSplitPoints, \n"
		"	uniform float enableShadows, \n";
		
		if (mParent->getShadowsFade() && mParent->getSceneManager()->getShadowFarDistance() > 0) 
		{
			outStream << 
				"	uniform float3 fadeStart_farDist, \n";
		}
	}
}

//----------------------------------------------------------------------------------------

void MaterialGenerator::fpCalcShadowSource(Ogre::StringUtil::StrStreamType& outStream)
{
	if (needShadows() || needTerrainLightMap())
		outStream <<
		"	float shadowing; \n";
		
	if (needShadows())
	{
		outStream <<
		"	float shadowingRT;";
		
		outStream << "\n		";
		for (int i = 0; i < mParent->getNumShadowTex(); ++i)
			outStream << "float4 invShadowMapSize" << i << "; ";
			
		outStream <<
		"	shadowingRT = calcPSSMShadow(";
		for (int i=0; i<mParent->getNumShadowTex(); ++i)
			outStream << "shadowMap"+toStr(i)+", ";
		for (int i=0; i<mParent->getNumShadowTex(); ++i)
			outStream << "lightPosition"+toStr(i)+", ";
		for (int i=0; i<mParent->getNumShadowTex(); ++i)
			outStream << "invShadowMapSize"+toStr(i)+", ";
		
		outStream <<
		"pssmSplitPoints, texCoord.z); \n";
		
		if (mParent->getShadowsFade() && mParent->getSceneManager()->getShadowFarDistance() > 0) outStream <<
			"	float fadestart = fadeStart_farDist.x; \n"
			"	float fardist = fadeStart_farDist.y; \n"
			"	float fade = saturate(((texCoord.z/fardist) - (1-(fadestart)))/(fadestart));"
			"	shadowingRT = lerp(shadowingRT, 1, fade * fadeStart_farDist.z); \n";
			
		outStream <<
		"	shadowingRT = 1-(1-shadowingRT)*enableShadows; \n";
		
	}
	
	if (needTerrainLightMap())
	{
		outStream <<
		"	float shadowingLM; \n"
		"	float2 worldPos = float2(texCoord.w, iNormal.w); \n" // get world position
		"	float2 lmTexCoord = (worldPos / terrainWorldSize) + 0.5; \n" // convert to image space 0..1
		"	shadowingLM = tex2D(terrainLightMap, lmTexCoord).x; \n" // fetch texture r channel
		"	if (enableTerrainLightMap == 0.f) shadowingLM = 1.f; \n";
	}
	
	// put together realtime and static shadow
	if (needShadows() || needTerrainLightMap())
	{
		if (needShadows() && needTerrainLightMap()) outStream <<
		"	shadowing = min(shadowingRT, shadowingLM); \n";
		else if (needShadows()) outStream <<
		"	shadowing = shadowingRT; \n";
		else /* if (needTerrainLightMap()) */ outStream <<
		"	shadowing = shadowingLM; \n";
	}
}

//----------------------------------------------------------------------------------------

void MaterialGenerator::generateFragmentProgramSource(Ogre::StringUtil::StrStreamType& outStream)
{
	if (needShadows())
		fpRealtimeShadowHelperSource(outStream);
	mTexCoord_i=0;
	outStream <<
		"void main_fp("
		"	in float4 iPosition : POSITION, \n";
		
	if (fpNeedPos()) outStream <<
		"	in float4 position : COLOR, \n";
		
	outStream <<
		"	in float4 texCoord : TEXCOORD"+toStr(mTexCoord_i++)+", \n";
	
	if (mShader->vertexColour) outStream <<
		"	in float4 vertexColour : TEXCOORD"+toStr(mTexCoord_i++)+", \n";
	
	if (mShader->wind == 1) outStream <<
		"	in float alphaFade : TEXCOORD"+toStr(mTexCoord_i++)+", \n";
	
	if (fpNeedNormal()) 
	{
		if(UsePerPixelNormals())
		{
			outStream <<"	in float4 pNormal : COLOR1, \n";
		}
		else
		{
			outStream <<"	in float4 iNormal : TEXCOORD"+toStr(mTexCoord_i++)+", \n";
		}
	}
	
	if (fpNeedEyeVector()) outStream <<
		"	in float4 inEyeVector : TEXCOORD"+toStr(mTexCoord_i++)+", \n";
		
	if (needNormalMap() || fp_need_tangent()) outStream <<
		"	in float4 tangent : TEXCOORD"+toStr(mTexCoord_i++)+", \n"
		"	uniform float bumpScale, \n";

	if (MRTSupported()) 
	{
		outStream <<
		"	uniform float4x4 vMat, \n";
		if(!UsePerPixelNormals())
		{
			outStream << "	in float4 viewNormal : TEXCOORD"+ toStr( mTexCoord_i++ ) +", \n";
			//outStream << "	in float4 viewPosition : TEXCOORD"+ toStr( mTexCoord_i++ ) +", \n";
		}
	}

	outStream <<
	"	in float fogAmount : FOG, \n";

	if (vpNeedWvMat()) outStream <<
		"	uniform float4x4 wvMat, \n";
	
	if(MRTSupported())
	{
		outStream <<	"	uniform float4x4 wMat, \n";
	}
	
	if (fpNeedNormal()) outStream <<
		"	uniform float4x4 wITMat, \n";

	if (needFresnel()) outStream <<
		"	uniform float fresnelBias, \n"
		"	uniform float fresnelScale, \n"
		"	uniform float fresnelPower, \n";
		
	if (needDiffuseMap()) outStream <<
		"	uniform sampler2D diffuseMap : TEXUNIT"+toStr(mDiffuseTexUnit)+", \n";
	
	if (needLightMap()) outStream <<
		"	uniform sampler2D lightMap : TEXUNIT"+toStr(mLightTexUnit)+", \n";
		
	if (needTerrainLightMap()) outStream <<
		"	uniform sampler2D terrainLightMap : TEXUNIT"+toStr(mTerrainLightTexUnit)+", \n"
		"	uniform float enableTerrainLightMap, \n"
		"	uniform float terrainWorldSize, \n";

	if (needBlendMap()) outStream <<
		"	uniform sampler2D blendMap : TEXUNIT"+toStr(mBlendTexUnit)+", \n";
		
	if (needAlphaMap()) outStream <<
		"	uniform sampler2D alphaMap	 : TEXUNIT"+toStr(mAlphaTexUnit)+", \n";
		
	if (needNormalMap()) outStream <<
		"	uniform sampler2D normalMap  : TEXUNIT"+toStr(mNormalTexUnit)+", \n";
		
	if (needEnvMap()) outStream << 
		"	uniform samplerCUBE envMap : TEXUNIT"+toStr(mEnvTexUnit)+", \n";
	if (needEnvMap() && !needFresnel()) outStream << 
		"	uniform float reflAmount, \n";
		
	if (needSpecMap()) outStream <<
		"	uniform sampler2D specMap : TEXUNIT"+toStr(mSpecTexUnit)+", \n";
		
	if (needReflectivityMap()) outStream <<
		"	uniform sampler2D reflectivityMap : TEXUNIT"+toStr(mReflTexUnit)+", \n";
	
	if (mShader->parallax) outStream <<
		"	uniform float parallaxHeight, \n";
	
	fpShadowingParams(outStream);
		
	// lighting params
	// only 1 directional light is supported
	if (fpNeedLighting())
	{
		outStream <<
		// light
		"	uniform float3 lightDiffuse, \n"
		"	uniform float3 lightSpecular, \n"
		"	uniform float4 lightPosition, \n"
		"	uniform float3 globalAmbient, \n"
		// material
		"	uniform float3 matAmbient, \n"
		"	uniform float3 matDiffuse, \n";
		if (!needSpecMap()) outStream <<
		"	uniform float4 matSpecular, \n"; // shininess in w
    if(fp_need_ward_aniso()) //--------
      outStream << "	uniform float2 aniso_roughness, \n";
	}
	outStream <<
		"	uniform float3 fogColor, \n";
		
	if (mDef->mProps->transparent && needLightingAlpha())
		outStream <<
		"	uniform float4 lightingAlpha, \n";
		
	if (mDef->mProps->transparent) outStream <<
		"	uniform float alphaRejectValue, \n";
	
	
	if(MRTSupported())
	{
		outStream << "	out float4 oColor : COLOR0, \n";
		outStream << "	out float4 oColor1 : COLOR1, \n";
		outStream << "	out float4 oColor2 : COLOR2, \n";
		outStream << "	uniform float far \n";
	}
	else
	{
			outStream << "	out float4 oColor : COLOR \n";
	}
	outStream << 	") \n"
		"{ \n";
	
	// calc shadowing
	fpCalcShadowSource(outStream);
	
	// fetch diffuse texture
	if (needDiffuseMap()) outStream <<
		"	float4 diffuseTex = tex2D(diffuseMap, texCoord.xy); \n";
	else if (needLightMap() && needBlendMap())
	{
		outStream <<
		"	float3 lightTex = tex2D(lightMap, texCoord.xy).x * matDiffuse.xyz; \n"
		"	float4 blendTex = tex2D(blendMap, texCoord.xy); \n"
		"	float4 diffuseTex = float4( lerp(lightTex.xyz, blendTex.xyz, blendTex.a), 1 ); \n";
	}

    
  // get TBN matrix for normal / parallax mapping
  if (needNormalMap() || mShader->parallax) 
  {
    outStream << "	float3x3 tbn = float3x3( tangent.xyz * bumpScale, cross( tangent.xyz, iNormal.xyz ) * bumpScale, iNormal.xyz ); \n";
  }
		
	if (mShader->parallax && needDiffuseMap())
	{
		outStream <<
		// convert view vector to tangent space using TBN matrix
		"	float3 tsViewVec = normalize( mul( tbn, inEyeVector.xyz ) ); \n"
		//"	float clamp = 2.f; \n"
		"	float height = diffuseTex.w; \n"
		"	float offset = parallaxHeight * ( height * 2.0f - 1.0f ); \n"
		// shift UV
    " float2 tmp_tex_coord = texCoord.xy; \n"
		"	texCoord.xy += offset * tsViewVec.xy; \n"
		// re-fetch diffuse texture using the modulated UV
		"	diffuseTex = tex2D( diffuseMap, texCoord.xy ); \n";

    if(need_refinement())
    {
      outStream <<
      " height += diffuseTex.w; \n"
      " offset = parallaxHeight * ( height - 1.0 ); \n"
      " texCoord.xy = tmp_tex_coord + offset * tsViewVec.xy; \n"
      " diffuseTex = tex2D( diffuseMap, texCoord.xy ); \n";
    }
	}
	
	if (fpNeedEyeVector()) 
  {
    outStream <<
		"	float3 v = -normalize( inEyeVector.xyz ); \n"; // normalize in the pixel shader for higher accuracy (inaccuracies would be caused by vertex interpolation)
  }

  if (fpNeedNormal())
	{
    outStream << "  float3 n = float3( 0.0f ); \n";
  
    if (needNormalMap())
    { 
      outStream <<
	    "	float4 normalTex = tex2D( normalMap, texCoord.xy ); \n"
			"	n = mul( transpose(tbn), normalTex.xyz * 2.f - 1.f ); \n"
			"	n = mul( (float3x3)wITMat, n ); \n";
    }
		else 
    {    
      outStream <<
			"	n = mul( (float3x3)wITMat, iNormal.xyz ); \n";
    }
    
    outStream << 
    "  n = normalize( n ); \n";
  }

  if(fpNeedEyeVector() && fpNeedNormal())
  {
    outStream << "  float n_dot_v = dot( n, v ); \n"; //-------- N dot V
  }

	// calculate lighting (per-pixel)
	if (fpNeedLighting())
	{
		outStream <<	
		// compute the diffuse term
		"	float3 l = normalize( lightPosition.xyz ); \n"
		"	float n_dot_l = max( dot( n, l ), 0.0f ); \n"; // N dot L
		
		if ((needLightMap() && needBlendMap())) 
    {
      outStream <<
			"	float3 diffuse = lightDiffuse.xyz * diffuseTex.xyz; \n";
    }
		else if (needDiffuseMap()) 
    {
      outStream <<
			"	float3 diffuse = matDiffuse.xyz * lightDiffuse.xyz * diffuseTex.xyz; \n";
    }
		else 
    {
      outStream <<
			"	float3 diffuse = matDiffuse.xyz * lightDiffuse.xyz; \n";
    }
			
		// compute the specular term
		if (needSpecMap()) 
    {
      outStream <<
			"	float4 specTex = tex2D( specMap, texCoord.xy ); \n"
			"	float3 matSpec = specTex.xyz; \n"
			"	float shininess = specTex.w; \n";
    }
		else 
    {
      outStream <<
			"	float3 matSpec = matSpecular.xyz; \n"
			"	float shininess = matSpecular.w / 255.0f; \n";
    }

    outStream << 
    "  float3 result = float3( 0 ); \n" //-------- this will contain the lighting result
    "  if( n_dot_l > 0.0 ) \n"
    "  { \n";
    if(fp_need_tangent())
    {
      outStream << 
      "    float3 t = normalize( mul( (float3x3)wITMat, tangent.xyz + ( n - iNormal.xyz ).yzx ) ); \n"
      "    float3 b = normalize( cross( t, n ) ); \n";
    }
          
    outStream <<
		"    float3 h = normalize( l + v ); \n" 
    "    float n_dot_h = dot( n, h ); \n" //-------- N dot H 
    "    float3 specular_term = matSpec * lightSpecular.xyz; \n";

    if(fp_need_ward_iso())
    {
      outStream <<
      // WARD ISO 1992
      /**/
      "    float roughness_sq = shininess * shininess + 0.00001f; \n" //-------- no division by 0
      //"    float roughness_sq = 0.04f + 0.00001f; \n" //-------- no division by 0
      "    float beta = -pow( tan( acos( n_dot_h ) ), 2.0f ); \n"
      "    float denom = 3.14159f * roughness_sq; \n"
      "    float numer = exp( beta / roughness_sq ); \n"
      "    denom *= 4.0f * sqrt( n_dot_l * n_dot_v ); \n"
      "    float3 specular = specular_term * ( numer / denom ); \n"     
      "    result += n_dot_l * ( diffuse + specular ); \n"; //--------
      /**/
    }

    if(fp_need_ward_aniso())
    {
      outStream <<
      // WARD ANISO MORODER 2010
      /**/
      "    float h_dot_t = dot( h, t ); \n"
      "    float h_dot_b = dot( h, b ); \n"
      "    float2 roughness = aniso_roughness + float2( 1e-5f, 1e-5f ); \n"
      "    float2 beta_coeff = float2( h_dot_t, h_dot_b ) / roughness; \n"
      "    float beta = -2.0f * ( dot( beta_coeff, beta_coeff ) / ( 1.0f + n_dot_h ) ); \n"
      "    float denom = 3.14159f * roughness.x * roughness.y; \n"
      "    float numer = exp( beta ); \n"
      "    denom *= pow( n_dot_l + n_dot_v, 4.0f ); \n"
      "    float coeff_1 = dot( -b, l ) * dot( b, v ); \n"
      "    float coeff_2 = n_dot_l * n_dot_v; \n"
      "    numer *= 2.0f * ( 1.0f + coeff_2 + coeff_1 * ( coeff_2 + coeff_1 ) ); \n"
      "    float3 specular = specular_term * ( numer / denom ); \n"
      "    result += n_dot_l * ( diffuse + specular ); \n";
      /**/
    }

    if(fp_need_cook_torrance())
    {
      outStream <<
      // COOK-TORRANCE 1982
      /**/
      "    float h_dot_v = dot( h, v ); \n"
      "    float geo_numer = 2.0f * n_dot_h; \n"
      "    float geo_denom = 1.0f / h_dot_v; \n"
      "    float geo_b = ( geo_numer * n_dot_v ) * geo_denom; \n"
      "    float geo_c = ( geo_numer * n_dot_l ) * geo_denom; \n"
      "    float geo = min( 1.0f, min( geo_b, geo_c ) ); \n"
      //  Beckmann roughness
      "    float roughness_sq = shininess * shininess + 0.00001f; \n"
      "    float roughness_a = 1.0f / ( 4.0f * roughness_sq * pow( n_dot_h, 4.0f ) ); \n"
      "    float roughness_b = n_dot_h * n_dot_h - 1.0f; \n"
      "    float roughness_c = roughness_sq * n_dot_h * n_dot_h; \n"
      "    float roughness = roughness_a * exp( roughness_b / roughness_c ); \n"
      "    float3 specular = specular_term * ( ( geo * roughness ) / ( n_dot_v * n_dot_l ) ); \n"
      "    result += n_dot_l * ( diffuse + specular ); \n";
      /**/
    }

    if(fp_need_phong())
    {
      outStream <<
      // PHONG 1973
      /**/
      "    float3 r = normalize( 2.0f * n * n_dot_l - l ); \n"
      "    float r_dot_v = max( dot( r, v ), 0.0f ); \n"
      "    float3 specular = specular_term * pow( r_dot_v, shininess * 255.0f ); \n"
      "    result += n_dot_l * diffuse + specular; \n";
      /**/
    }

    if(fp_need_blinn_phong())
    {
      outStream <<
      // BLINN-PHONG 1977
      /**/
      "    float3 specular = specular_term * pow( n_dot_h, shininess * 255.0f ); \n"
      "    result += n_dot_l * diffuse + specular; \n";
      /**/
    }
    
    outStream <<
    "  } \n";

    //"  } \n"
    //"  result = t; \n";

		// compute the ambient term
		outStream << "	float3 ambient = matAmbient.xyz * globalAmbient.xyz ";
		if (needDiffuseMap() || (needLightMap() && needBlendMap())) 
    {
      outStream <<	"* diffuseTex.xyz";
    }
		outStream << "; \n";

		// add all terms together (also with shadow)
		if (needShadows() || needTerrainLightMap()) 
    {
      outStream <<
      "	float3 lightColour = ambient + result * shadowing; \n"; //--------
      //"	float3 lightColour = result; \n"; //--------
    }
		else  
    {
      outStream <<
      "	float3 lightColour = ambient + result; \n"; //--------
      //"	float3 lightColour = result; \n"; //--------
    }
	}
	
	// cube reflection
	if (needEnvMap())
	{
		if (needFresnel())
		{
			outStream <<
			"	float facing = 1.0 - max(abs( n_dot_v ), 0); \n";
			if (!needReflectivityMap()) outStream <<
				"	float reflectionFactor = saturate(fresnelBias + fresnelScale * pow(facing, fresnelPower)); \n";
			else outStream <<
				"	float reflectionFactor = tex2D(reflectivityMap, texCoord.xy).r * saturate(fresnelBias + fresnelScale * pow(facing, fresnelPower)); \n";
		}
		else
		{
			if (!needReflectivityMap()) outStream <<
				"	float reflectionFactor = reflAmount; \n";
			else outStream <<
				"	float reflectionFactor = tex2D(reflectivityMap, texCoord.xy).r * reflAmount; \n";
		}
		outStream << 
		"	float3 r = reflect( -v, n ); \n" // calculate reflection vector
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
		else if (needShadows() || needTerrainLightMap()) outStream << // shadows, but no lighting
		"	float4 color1 = diffuseTex * (0.65f + 0.35f * shadowing); \n";
		else outStream <<
		"	float4 color1 = diffuseTex; \n";
	}
	
	if (mShader->vertexColour) outStream <<
		"	color1 *= vertexColour; \n";
	
	// add fog
	outStream <<
		"	oColor = lerp(color1, float4(fogColor,1), fogAmount); \n";
    //" oColor = float4(lightColour, 1); \n";
	
	// debug colour output  ------------------------------------------
	
	// world position
	//if (fpNeedPos()) outStream <<
	//"	oColor = float4(worldPosition.x, 1, worldPosition.z, 1); \n";
	
	// normal
	// if (fpNeedNormal()) outStream <<
	//"	oColor = float4(n.x, n.y, n.z, 1); \n";
	
	// spec
	//outStream <<
	//"	oColor = float4(specularLight, 0.0, 0.0, 1.0); \n";
	
	// ---------------------------------------------------------------
		
	// alpha
	if (mDef->mProps->transparent)
	{
		if (needAlphaMap()) outStream <<
			"	float alpha = tex2D(alphaMap, float2(texCoord.x,texCoord.y*0.01f)).r; \n"; // use only r channel
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
		
		// discard rejected alpha pixels
		outStream << 
			"	clip( alpha<alphaRejectValue ? -1:1); \n";
		
		if (mShader->wind == 1) outStream <<
			"	alpha *= alphaFade; \n";
		
		outStream << 
		"	oColor.w = alpha; \n";
	}
	
	if(MRTSupported())
	{
		//outStream <<  "float4 viewPosition = mul(vMat, float4(worldPosition.xyz,1.0)); \n";
		
		if (fpNeedNormal() && (!(needEnvMap() || needNormalMap() || fpNeedLighting()))) outStream <<
			"	float3 viewPosition = float3(iNormal.z, position.w, viewNormal.w); \n";
		else outStream <<
			"	float3 viewPosition = float3(inEyeVector.w, position.w, viewNormal.w); \n";
			
		if(UsePerPixelNormals())
		{
			outStream <<  "float4 viewNormal = mul(wvMat, pNormal); \n";
		}
		outStream <<  "oColor1 = float4(length(viewPosition) / far, normalize(viewNormal.xyz).xyz); \n";
		//if(mDef->mProps->transparent)
		//{
			// multiply alpha
		//	outStream << "oColor1 = oColor1 * alpha;";    
		//}
		outStream <<  "	float depth = texCoord.z / far; \n";
		//oColor2.y is occluders for godrays
		if(StringUtil::startsWith(this->mDef->getName(), "sky/"))
		{
			outStream <<  "float Luminance = (0.2126*diffuseTex.r) + (0.7152*diffuseTex.g) + (0.0722*diffuseTex.b); \n";	
			outStream <<  "oColor2 = float4(depth,Luminance*0.3,1,1); \n";	
		}
		else
		{
			outStream <<  "oColor2 = float4(depth,0,1,1); \n";
		}
	}
	outStream << 
		"} \n";
}

//----------------------------------------------------------------------------------------

void MaterialGenerator::fragmentProgramParams(HighLevelGpuProgramPtr program)
{
	GpuProgramParametersSharedPtr params = program->getDefaultParameters();
	
	#ifndef _DEBUG
	params->setIgnoreMissingParams(true);
	#endif

	if (fpNeedLighting())
	{
		params->setNamedAutoConstant("lightDiffuse", GpuProgramParameters::ACT_LIGHT_DIFFUSE_COLOUR, 0);
		params->setNamedAutoConstant("lightPosition", GpuProgramParameters::ACT_LIGHT_POSITION, 0);
		params->setNamedAutoConstant("matAmbient", GpuProgramParameters::ACT_SURFACE_AMBIENT_COLOUR);
		params->setNamedAutoConstant("matDiffuse", GpuProgramParameters::ACT_SURFACE_DIFFUSE_COLOUR);
		if (!needSpecMap())
			params->setNamedAutoConstant("matSpecular", GpuProgramParameters::ACT_SURFACE_SPECULAR_COLOUR);
		params->setNamedAutoConstant("globalAmbient", GpuProgramParameters::ACT_AMBIENT_LIGHT_COLOUR);
	}
	
	params->setNamedAutoConstant("fogColor", GpuProgramParameters::ACT_FOG_COLOUR);
	
	#ifndef _DEBUG  // issue in debug
	if (vpNeedWvMat())
		params->setNamedAutoConstant("wvMat", GpuProgramParameters::ACT_WORLDVIEW_MATRIX);
	#endif
		
	if (fpNeedNormal())
		params->setNamedAutoConstant("wITMat", GpuProgramParameters::ACT_INVERSE_TRANSPOSE_WORLD_MATRIX);

	if(MRTSupported())
	{
		//params->setNamedAutoConstant("vMat", GpuProgramParameters::ACT_VIEW_MATRIX);
		params->setNamedAutoConstant("far", GpuProgramParameters::ACT_FAR_CLIP_DISTANCE);
	}
		
	if (needTerrainLightMap())
		params->setNamedConstant("enableTerrainLightMap", Real(1));
	
	individualFragmentProgramParams(params);
}

//----------------------------------------------------------------------------------------

void MaterialGenerator::individualFragmentProgramParams(Ogre::GpuProgramParametersSharedPtr params)
{
	//#ifndef _DEBUG
	params->setIgnoreMissingParams(true);
	//#endif
	
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
	
	if (needNormalMap())
		params->setNamedConstant("bumpScale", mDef->mProps->bumpScale);
	
	if (mDef->mProps->transparent)
		params->setNamedConstant("alphaRejectValue", Real(float(mDef->mProps->alphaRejectValue)/float(256.0f)));

	if (needLightingAlpha())
		params->setNamedConstant("lightingAlpha", mDef->mProps->lightingAlpha);
		
	if (fpNeedLighting())
		params->setNamedConstant("lightSpecular", mDef->mProps->specular);
		
	if (mShader->parallax)
		params->setNamedConstant("parallaxHeight", mDef->mProps->parallaxHeight);

	if (fp_need_ward_aniso())
		params->setNamedConstant("aniso_roughness", Vector4(mDef->mProps->aniso_roughness[0], mDef->mProps->aniso_roughness[1], 0, 0));

	if (needShadows())
	{
		params->setNamedConstant("enableShadows", 1.f);
		params->setNamedConstant("pssmSplitPoints", mParent->pApp->splitPoints);
		//for (int i=0; i<mParent->getNumShadowTex(); ++i)
		//	params->setNamedAutoConstant("invShadowMapSize"+toStr(i), GpuProgramParameters::ACT_INVERSE_TEXTURE_SIZE, i+mShadowTexUnit_start);
	}
	
	if (needTerrainLightMap())
		params->setNamedConstant("terrainWorldSize", Real(1025)); // real value set later in changeShadows()
}

//----------------------------------------------------------------------------------------
