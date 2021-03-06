#include "pch.h"
#include "Defines.h"
#include "TracksXml.h"
#include "tinyxml.h"

using namespace Ogre;


TrackInfo::TrackInfo()
	:n(-1),crtver(0.0),name("none"),scenery("none"),author("none")
	,fluids(0),bumps(0),jumps(0),loops(0),pipes(0),banked(0),frenzy(0),longn(0)
	,diff(0),rating(0), rateuser(0),drivenlaps(0)
{	}


Date s2dt(const char* a)
{
	Date d;  d.day=0; d.month=0; d.year=0;
	TIXML_SSCANF(a,"%2d-%2d-%2d",&d.day,&d.month,&d.year);
	return d;
}
std::string dt2s(const Date& dt)
{
	return fToStr(dt.day,0,2)+"-"+fToStr(dt.month,0,2)+"-"+fToStr(dt.year,0,2);
}


//  Load
//--------------------------------------------------------------------------------------------------------------------------------------

bool TracksXml::LoadXml(Ogre::String file)
{
	TiXmlDocument doc;
	if (!doc.LoadFile(file.c_str()))  return false;
		
	TiXmlElement* root = doc.RootElement();
	if (!root)  return false;

	//  clear
	//Default();
	trks.clear();  trkmap.clear();

	///  tracks
	const char* a;  int i=1;  //0 = none
	TiXmlElement* eTrk = root->FirstChildElement("track");
	while (eTrk)
	{
		TrackInfo t;
		a = eTrk->Attribute("n");			if (a)  t.n = s2i(a);
		a = eTrk->Attribute("name");		if (a)  t.name = std::string(a);
		a = eTrk->Attribute("created");		if (a)  t.created = s2dt(a);
		a = eTrk->Attribute("crtver");		if (a)  t.crtver = s2r(a);
		a = eTrk->Attribute("modified");	if (a)  t.modified = s2dt(a);
		a = eTrk->Attribute("scenery");		if (a)  t.scenery = std::string(a);
		a = eTrk->Attribute("author");		if (a)  t.author = std::string(a);

		a = eTrk->Attribute("fluids");		if (a)  t.fluids = s2i(a);
		a = eTrk->Attribute("bumps");		if (a)  t.bumps = s2i(a);	a = eTrk->Attribute("jumps");		if (a)  t.jumps = s2i(a);
		a = eTrk->Attribute("loops");		if (a)  t.loops = s2i(a);	a = eTrk->Attribute("pipes");		if (a)  t.pipes = s2i(a);
		a = eTrk->Attribute("banked");		if (a)  t.banked = s2i(a);	a = eTrk->Attribute("frenzy");		if (a)  t.frenzy = s2i(a);
		a = eTrk->Attribute("long");		if (a)  t.longn = s2i(a);

		a = eTrk->Attribute("diff");		if (a)  t.diff = s2i(a);
		a = eTrk->Attribute("rating");		if (a)  t.rating = s2i(a);
		a = eTrk->Attribute("rateuser");	if (a)  t.rateuser = s2i(a);
		a = eTrk->Attribute("drivenlaps");	if (a)  t.drivenlaps = s2i(a);

		trks.push_back(t);
		//trkmap[t.name] = &trks.back();
		trkmap[t.name] = i++;
		eTrk = eTrk->NextSiblingElement("track");
	}
	return true;
}


//  Save
//--------------------------------------------------------------------------------------------------------------------------------------

bool TracksXml::SaveXml(Ogre::String file)
{
	TiXmlDocument xml;	TiXmlElement root("tracks");

	for (int i=0; i < trks.size(); ++i)
	{
		const TrackInfo& t = trks[i];
		TiXmlElement trk("track");

		trk.SetAttribute("n",			toStrC( t.n ));
		trk.SetAttribute("name",		t.name.c_str());
		trk.SetAttribute("created",		dt2s(t.created).c_str());
		trk.SetAttribute("crtver",		toStrC( t.crtver ));
		trk.SetAttribute("modified",	dt2s(t.modified).c_str());
		trk.SetAttribute("scenery",		t.scenery.c_str());
		trk.SetAttribute("author",		t.author.c_str());
		
		trk.SetAttribute("fluids",		toStrC( t.fluids ));
		trk.SetAttribute("bumps",		toStrC( t.bumps ));		trk.SetAttribute("jumps",		toStrC( t.jumps ));
		trk.SetAttribute("loops",		toStrC( t.loops ));		trk.SetAttribute("pipes",		toStrC( t.pipes ));
		trk.SetAttribute("banked",		toStrC( t.banked ));	trk.SetAttribute("frenzy",		toStrC( t.frenzy ));
		trk.SetAttribute("long",		toStrC( t.longn ));

		trk.SetAttribute("diff",		toStrC( t.diff ));
		trk.SetAttribute("rating",		toStrC( t.rating ));
		trk.SetAttribute("rateuser",	toStrC( t.rateuser ));
		trk.SetAttribute("drivenlaps",	toStrC( t.drivenlaps ));
		root.InsertEndChild(trk);
	}

	xml.InsertEndChild(root);
	return xml.SaveFile(file.c_str());
}

//--------------------------------------------------------------------------------------------------------------------------------------
